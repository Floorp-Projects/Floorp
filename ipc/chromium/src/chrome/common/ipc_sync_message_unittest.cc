// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test to make sure that the serialization of synchronous IPC messages
// works.  This ensures that the macros and templates were defined correctly.
// Doesn't test the IPC channel mechanism.

#include <string.h>

#include "base/basictypes.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"


#define MESSAGES_INTERNAL_FILE "chrome/common/ipc_sync_message_unittest.h"
#include "chrome/common/ipc_message_macros.h"

static IPC::Message* g_reply;

class TestMessageReceiver {
 public:

  void On_0_1(bool* out1) {
    *out1 = false;
  }

  void On_0_2(bool* out1, int* out2) {
    *out1 = true;
    *out2 = 2;
  }

  void On_0_3(bool* out1, int* out2, std::string* out3) {
    *out1 = false;
    *out2 = 3;
    *out3 = "0_3";
  }

  void On_1_1(int in1, bool* out1) {
    DCHECK(in1 == 1);
    *out1 = true;
  }

  void On_1_2(bool in1, bool* out1, int* out2) {
    DCHECK(!in1);
    *out1 = true;
    *out2 = 12;
  }

  void On_1_3(int in1, std::string* out1, int* out2, bool* out3) {
    DCHECK(in1 == 3);
    *out1 = "1_3";
    *out2 = 13;
    *out3 = false;
  }

  void On_2_1(int in1, bool in2, bool* out1) {
    DCHECK(in1 == 1 && !in2);
    *out1 = true;
  }

  void On_2_2(bool in1, int in2, bool* out1, int* out2) {
    DCHECK(!in1 && in2 == 2);
    *out1 = true;
    *out2 = 22;
  }

  void On_2_3(int in1, bool in2, std::string* out1, int* out2, bool* out3) {
    DCHECK(in1 == 3 && in2);
    *out1 = "2_3";
    *out2 = 23;
    *out3 = false;
  }

  void On_3_1(int in1, bool in2, std::string in3, bool* out1) {
    DCHECK(in1 == 1 && !in2 && in3 == "3_1");
    *out1 = true;
  }

  void On_3_2(std::string in1, bool in2, int in3, bool* out1, int* out2) {
    DCHECK(in1 == "3_2" && !in2 && in3 == 2);
    *out1 = true;
    *out2 = 32;
  }

  void On_3_3(int in1, std::string in2, bool in3, std::string* out1, int* out2,
              bool* out3) {
    DCHECK(in1 == 3 && in2 == "3_3" && in3);
    *out1 = "3_3";
    *out2 = 33;
    *out3 = false;
  }

  bool Send(IPC::Message* message) {
    // gets the reply message, stash in global
    DCHECK(g_reply == NULL);
    g_reply = message;
    return true;
  }

  void OnMessageReceived(const IPC::Message& msg) {
    IPC_BEGIN_MESSAGE_MAP(TestMessageReceiver, msg)
      IPC_MESSAGE_HANDLER(Msg_C_0_1, On_0_1)
      IPC_MESSAGE_HANDLER(Msg_C_0_2, On_0_2)
      IPC_MESSAGE_HANDLER(Msg_C_0_3, On_0_3)
      IPC_MESSAGE_HANDLER(Msg_C_1_1, On_1_1)
      IPC_MESSAGE_HANDLER(Msg_C_1_2, On_1_2)
      IPC_MESSAGE_HANDLER(Msg_C_1_3, On_1_3)
      IPC_MESSAGE_HANDLER(Msg_C_2_1, On_2_1)
      IPC_MESSAGE_HANDLER(Msg_C_2_2, On_2_2)
      IPC_MESSAGE_HANDLER(Msg_C_2_3, On_2_3)
      IPC_MESSAGE_HANDLER(Msg_C_3_1, On_3_1)
      IPC_MESSAGE_HANDLER(Msg_C_3_2, On_3_2)
      IPC_MESSAGE_HANDLER(Msg_C_3_3, On_3_3)
      IPC_MESSAGE_HANDLER(Msg_R_0_1, On_0_1)
      IPC_MESSAGE_HANDLER(Msg_R_0_2, On_0_2)
      IPC_MESSAGE_HANDLER(Msg_R_0_3, On_0_3)
      IPC_MESSAGE_HANDLER(Msg_R_1_1, On_1_1)
      IPC_MESSAGE_HANDLER(Msg_R_1_2, On_1_2)
      IPC_MESSAGE_HANDLER(Msg_R_1_3, On_1_3)
      IPC_MESSAGE_HANDLER(Msg_R_2_1, On_2_1)
      IPC_MESSAGE_HANDLER(Msg_R_2_2, On_2_2)
      IPC_MESSAGE_HANDLER(Msg_R_2_3, On_2_3)
      IPC_MESSAGE_HANDLER(Msg_R_3_1, On_3_1)
      IPC_MESSAGE_HANDLER(Msg_R_3_2, On_3_2)
      IPC_MESSAGE_HANDLER(Msg_R_3_3, On_3_3)
    IPC_END_MESSAGE_MAP()
  }

};

void Send(IPC::SyncMessage* msg) {
  static TestMessageReceiver receiver;

  IPC::MessageReplyDeserializer* reply_serializer = msg->GetReplyDeserializer();
  DCHECK(reply_serializer != NULL);

  // "send" the message
  receiver.OnMessageReceived(*msg);
  delete msg;

  // get the reply message from the global, and deserialize the output
  // parameters into the output pointers.
  DCHECK(g_reply != NULL);
  bool result = reply_serializer->SerializeOutputParameters(*g_reply);
  DCHECK(result);
  delete g_reply;
  g_reply = NULL;
  delete reply_serializer;
}

TEST(IPCSyncMessageTest, Main) {
  bool bool1 = true;
  int int1 = 0;
  std::string string1;

  Send(new Msg_C_0_1(&bool1));
  DCHECK(!bool1);

  Send(new Msg_C_0_2(&bool1, &int1));
  DCHECK(bool1 && int1 == 2);

  Send(new Msg_C_0_3(&bool1, &int1, &string1));
  DCHECK(!bool1 && int1 == 3 && string1 == "0_3");

  bool1 = false;
  Send(new Msg_C_1_1(1, &bool1));
  DCHECK(bool1);

  bool1 = false;
  Send(new Msg_C_1_2(false, &bool1, &int1));
  DCHECK(bool1 && int1 == 12);

  bool1 = true;
  Send(new Msg_C_1_3(3, &string1, &int1, &bool1));
  DCHECK(string1 == "1_3" && int1 == 13 && !bool1);

  bool1 = false;
  Send(new Msg_C_2_1(1, false, &bool1));
  DCHECK(bool1);

  bool1 = false;
  Send(new Msg_C_2_2(false, 2, &bool1, &int1));
  DCHECK(bool1 && int1 == 22);

  bool1 = true;
  Send(new Msg_C_2_3(3, true, &string1, &int1, &bool1));
  DCHECK(string1 == "2_3" && int1 == 23 && !bool1);

  bool1 = false;
  Send(new Msg_C_3_1(1, false, "3_1", &bool1));
  DCHECK(bool1);

  bool1 = false;
  Send(new Msg_C_3_2("3_2", false, 2, &bool1, &int1));
  DCHECK(bool1 && int1 == 32);

  bool1 = true;
  Send(new Msg_C_3_3(3, "3_3", true, &string1, &int1, &bool1));
  DCHECK(string1 == "3_3" && int1 == 33 && !bool1);

  // Routed messages, just a copy of the above but with extra routing paramater
  Send(new Msg_R_0_1(0, &bool1));
  DCHECK(!bool1);

  Send(new Msg_R_0_2(0, &bool1, &int1));
  DCHECK(bool1 && int1 == 2);

  Send(new Msg_R_0_3(0, &bool1, &int1, &string1));
  DCHECK(!bool1 && int1 == 3 && string1 == "0_3");

  bool1 = false;
  Send(new Msg_R_1_1(0, 1, &bool1));
  DCHECK(bool1);

  bool1 = false;
  Send(new Msg_R_1_2(0, false, &bool1, &int1));
  DCHECK(bool1 && int1 == 12);

  bool1 = true;
  Send(new Msg_R_1_3(0, 3, &string1, &int1, &bool1));
  DCHECK(string1 == "1_3" && int1 == 13 && !bool1);

  bool1 = false;
  Send(new Msg_R_2_1(0, 1, false, &bool1));
  DCHECK(bool1);

  bool1 = false;
  Send(new Msg_R_2_2(0, false, 2, &bool1, &int1));
  DCHECK(bool1 && int1 == 22);

  bool1 = true;
  Send(new Msg_R_2_3(0, 3, true, &string1, &int1, &bool1));
  DCHECK(string1 == "2_3" && int1 == 23 && !bool1);

  bool1 = false;
  Send(new Msg_R_3_1(0, 1, false, "3_1", &bool1));
  DCHECK(bool1);

  bool1 = false;
  Send(new Msg_R_3_2(0, "3_2", false, 2, &bool1, &int1));
  DCHECK(bool1 && int1 == 32);

  bool1 = true;
  Send(new Msg_R_3_3(0, 3, "3_3", true, &string1, &int1, &bool1));
  DCHECK(string1 == "3_3" && int1 == 33 && !bool1);
}
