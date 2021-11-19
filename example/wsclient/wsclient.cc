#include "json/json.h"
#include <iostream>
#include <memory>

#include "../easywsclient/easywsclient.hpp"

const int kUserTypeWeb = 0;
const int kUserTypeStreamer = 1;

// NOTION: same with signal-server
const char kJoinRoom[] = "joinRoom";
const char kOffer[] = "offer";
const char kAnswer[] = "answer";
const char kCandidate[] = "candidate";
const char kHangUp[] = "hangUp";
const char kLeaveRoom[] = "leaveRoom";
const char kUpdateUserList[] = "updateUserList";

// NOTION: same with webrtc-streamer's peer_connection_manager.cc
// Names used for a IceCandidate JSON object.
const char* kCandidateSdpMidName = "sdpMid";
const char* kCandidateSdpMlineIndexName = "sdpMLineIndex";
const char* kCandidateSdpName = "candidate";

// Names used for a SessionDescription JSON object.
const char* kSessionDescriptionTypeName = "type";
const char* kSessionDescriptionSdpName = "sdp";

struct JoinRoomData {
    std::string name;
    std::string id;
    int userType;
    std::string roomId;
};

/*
struct SessionData {
    std::string from;
    std::string to;
    std::string sessionId;
    std::string roomId;
};
*/
struct OfferData {
    std::string from;
    std::string to;
    std::string sessionId;
    std::string roomId;
    // description
    std::string sub_type;
    std::string sub_sdp;
};

struct CandidateData {
    std::string from;
    std::string to;
    std::string sessionId;
    std::string roomId;
    //Json::Value candidate; {"sdpMid":"xx", "sdpMLineIndex":"xx", "candidate":"xx"}
    std::string sub_sdpMid;
    std::string sub_sdpMLineIndex;
    std::string sub_candidate;
};

std::string BuildJoinRoom(const std::string& id) {
  Json::Value root;
  Json::StreamWriterBuilder builder;
  root["type"] = kJoinRoom;
  root["data"]["userType"] = kUserTypeStreamer;
  root["data"]["id"] = id;
  return Json::writeString(builder, root);
}

Json::Value OnOffer(const OfferData& message, const Json::Value desc) {
  Json::Value root;
  root["type"] = kAnswer;
  root["data"]["from"] = message.to;
  root["data"]["to"] = message.from;
  root["data"]["sessionId"] = message.sessionId;
  root["data"]["roomId"] = message.roomId;
  root["data"]["description"] = desc;
  return root;
}

int HandleMessage(const std::string & rawJson) {
  const auto rawJsonLength = static_cast<int>(rawJson.length());
  JSONCPP_STRING err;
  Json::Value root;
  Json::CharReaderBuilder builder;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  if (!reader->parse(rawJson.c_str(), rawJson.c_str() + rawJsonLength, &root, &err)) {
    std::cout << "error" << std::endl;
  }

  std::string type = root["type"].asString();
  Json::Value data = root["data"];

  if (type == std::string(kJoinRoom)) {
    JoinRoomData message = {
      .name = data["name"].asString(),
      .id = data["id"].asString(),
      .userType = data["userType"].asInt(),
      .roomId = data["roomId"].asString(),
    };
  } else if (type == std::string(kOffer)) {
    OfferData message = {
      .from = data["from"].asString(),
      .to = data["to"].asString(),
      .sessionId = data["sessionId"].asString(),
      .roomId = data["roomId"].asString(),
      .sub_type = data["description"]["type"].asString(),
      .sub_sdp = data["description"]["sdp"].asString(),
    };

    // TODO
    OnOffer(message, data["description"]);
  } else if (type == std::string(kCandidate)) {
    CandidateData message = {
      .from = data["from"].asString(),
      .to = data["to"].asString(),
      .sessionId = data["sessionId"].asString(),
      .roomId = data["roomId"].asString(),
      .sub_sdpMid = data["candidate"][kCandidateSdpMidName].asString(),
      .sub_sdpMLineIndex = data["candidate"][kCandidateSdpMlineIndexName].asString(),
      .sub_candidate = data["candidate"][kCandidateSdpName].asString(),
    };
  } else {
    std::cout << "not supported!" << std::endl;
    assert(false);
  }
}

int main()
{
    using easywsclient::WebSocket;
#ifdef _WIN32
    INT rc;
    WSADATA wsaData;

    rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc) {
        printf("WSAStartup Failed.\n");
        return 1;
    }
#endif
    std::unique_ptr<WebSocket> ws(WebSocket::from_url("ws://localhost:8089/ws"));
    assert(ws);
    ws->send(BuildJoinRoom("21345"));
    while (ws->getReadyState() != WebSocket::CLOSED) {
        WebSocket::pointer wsp = &*ws; // <-- because a unique_ptr cannot be copied into a lambda
        ws->poll();
        ws->dispatch([wsp](const std::string & message) {
            printf(">>> %s\n", message.c_str());
            HandleMessage(message);
        });
    }
#ifdef _WIN32
    WSACleanup();
#endif
    // N.B. - unique_ptr will free the WebSocket instance upon return:
    return 0;
}
