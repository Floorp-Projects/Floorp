// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header is meant to be included in multiple passes, hence no traditional
// header guard.
//
// In the first pass, IPC_MESSAGE_MACROS_ENUMS should be defined, which will
// create enums for each of the messages defined with the IPC_MESSAGE_* macros.
//
// In the second pass, either IPC_MESSAGE_MACROS_DEBUGSTRINGS or
// IPC_MESSAGE_MACROS_CLASSES should be defined (if both, DEBUGSTRINGS takes
// precedence).  Only one .cc file should have DEBUGSTRINGS defined, as this
// will create helper functions mapping message types to strings.  Having
// CLASSES defined will create classes for each of the messages defined with
// the IPC_MESSAGE_* macros.
//
// "Sync" messages are just synchronous calls, the Send() call doesn't return
// until a reply comes back.  Input parameters are first (const TYPE&), and
// To declare a sync message, use the IPC_SYNC_ macros.  The numbers at the
// end show how many input/output parameters there are (i.e. 1_2 is 1 in, 2
// out). The caller does a Send([route id, ], in1, &out1, &out2).
// The receiver's handler function will be
//     void OnSyncMessageName(const type1& in1, type2* out1, type3* out2)
//
//
// A caller can also send a synchronous message, while the receiver can respond
// at a later time.  This is transparent from the sender's size.  The receiver
// needs to use a different handler that takes in a IPC::Message* as the output
// type, stash the message, and when it has the data it can Send the message.
//
// Use the IPC_MESSAGE_HANDLER_DELAY_REPLY macro instead of IPC_MESSAGE_HANDLER
//     IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SyncMessageName,
//                                     OnSyncMessageName)
//
// The handler function will look like:
//     void OnSyncMessageName(const type1& in1, IPC::Message* reply_msg);
//
// Receiver stashes the IPC::Message* pointer, and when it's ready, it does:
//     ViewHostMsg_SyncMessageName::WriteReplyParams(reply_msg, out1, out2);
//     Send(reply_msg);

#include "chrome/common/ipc_message_utils.h"

#ifndef MESSAGES_INTERNAL_FILE
#error This file should only be included by X_messages.h, which needs to define MESSAGES_INTERNAL_FILE first.
#endif

// Trick scons and xcode into seeing the possible real dependencies since they
// don't understand #include MESSAGES_INTERNAL_FILE. See http://crbug.com/7828
#if 0
#include "chrome/common/ipc_sync_message_unittest.h"
#include "chrome/common/plugin_messages_internal.h"
#include "chrome/common/render_messages_internal.h"
#include "chrome/common/devtools_messages_internal.h"
#include "chrome/test/automation/automation_messages_internal.h"
#include "chrome/common/worker_messages_internal.h"
#endif

#ifndef IPC_MESSAGE_MACROS_INCLUDE_BLOCK
#define IPC_MESSAGE_MACROS_INCLUDE_BLOCK

// Multi-pass include of X_messages_internal.h.  Preprocessor magic allows
// us to use 1 header to define the enums and classes for our render messages.
#define IPC_MESSAGE_MACROS_ENUMS
#include MESSAGES_INTERNAL_FILE

#define IPC_MESSAGE_MACROS_CLASSES
#include MESSAGES_INTERNAL_FILE

#ifdef IPC_MESSAGE_MACROS_LOG_ENABLED
#define IPC_MESSAGE_MACROS_LOG
#include MESSAGES_INTERNAL_FILE
#endif

#undef MESSAGES_INTERNAL_FILE
#undef IPC_MESSAGE_MACROS_INCLUDE_BLOCK

#endif


// Undefine the macros from the previous pass (if any).
#undef IPC_BEGIN_MESSAGES
#undef IPC_END_MESSAGES
#undef IPC_MESSAGE_CONTROL0
#undef IPC_MESSAGE_CONTROL1
#undef IPC_MESSAGE_CONTROL2
#undef IPC_MESSAGE_CONTROL3
#undef IPC_MESSAGE_CONTROL4
#undef IPC_MESSAGE_CONTROL5
#undef IPC_MESSAGE_ROUTED0
#undef IPC_MESSAGE_ROUTED1
#undef IPC_MESSAGE_ROUTED2
#undef IPC_MESSAGE_ROUTED3
#undef IPC_MESSAGE_ROUTED4
#undef IPC_MESSAGE_ROUTED5
#undef IPC_MESSAGE_ROUTED6
#undef IPC_SYNC_MESSAGE_CONTROL0_0
#undef IPC_SYNC_MESSAGE_CONTROL0_1
#undef IPC_SYNC_MESSAGE_CONTROL0_2
#undef IPC_SYNC_MESSAGE_CONTROL0_3
#undef IPC_SYNC_MESSAGE_CONTROL1_0
#undef IPC_SYNC_MESSAGE_CONTROL1_1
#undef IPC_SYNC_MESSAGE_CONTROL1_2
#undef IPC_SYNC_MESSAGE_CONTROL1_3
#undef IPC_SYNC_MESSAGE_CONTROL2_0
#undef IPC_SYNC_MESSAGE_CONTROL2_1
#undef IPC_SYNC_MESSAGE_CONTROL2_2
#undef IPC_SYNC_MESSAGE_CONTROL2_3
#undef IPC_SYNC_MESSAGE_CONTROL3_1
#undef IPC_SYNC_MESSAGE_CONTROL3_2
#undef IPC_SYNC_MESSAGE_CONTROL3_3
#undef IPC_SYNC_MESSAGE_CONTROL4_1
#undef IPC_SYNC_MESSAGE_CONTROL4_2
#undef IPC_SYNC_MESSAGE_ROUTED0_0
#undef IPC_SYNC_MESSAGE_ROUTED0_1
#undef IPC_SYNC_MESSAGE_ROUTED0_2
#undef IPC_SYNC_MESSAGE_ROUTED0_3
#undef IPC_SYNC_MESSAGE_ROUTED1_0
#undef IPC_SYNC_MESSAGE_ROUTED1_1
#undef IPC_SYNC_MESSAGE_ROUTED1_2
#undef IPC_SYNC_MESSAGE_ROUTED1_3
#undef IPC_SYNC_MESSAGE_ROUTED1_4
#undef IPC_SYNC_MESSAGE_ROUTED2_0
#undef IPC_SYNC_MESSAGE_ROUTED2_1
#undef IPC_SYNC_MESSAGE_ROUTED2_2
#undef IPC_SYNC_MESSAGE_ROUTED2_3
#undef IPC_SYNC_MESSAGE_ROUTED3_0
#undef IPC_SYNC_MESSAGE_ROUTED3_1
#undef IPC_SYNC_MESSAGE_ROUTED3_2
#undef IPC_SYNC_MESSAGE_ROUTED3_3
#undef IPC_SYNC_MESSAGE_ROUTED4_0
#undef IPC_SYNC_MESSAGE_ROUTED4_1
#undef IPC_SYNC_MESSAGE_ROUTED4_2

#if defined(IPC_MESSAGE_MACROS_ENUMS)
#undef IPC_MESSAGE_MACROS_ENUMS

// TODO(jabdelmalek): we're using the lowest 12 bits of type for the message
// id, and the highest 4 bits for the channel type.  This constrains us to
// 16 channel types (currently using 8) and 4K messages per type.  Should
// really make type be 32 bits, but then we break automation with older Chrome
// builds..
// Do label##PreStart so that automation messages keep the same id as before.
#define IPC_BEGIN_MESSAGES(label) \
  enum label##MsgType { \
  label##Start = label##MsgStart << 12, \
  label##PreStart = (label##MsgStart << 12) - 1,

#define IPC_END_MESSAGES(label) \
  label##End \
  };

#define IPC_MESSAGE_CONTROL0(msg_class) \
  msg_class##__ID,

#define IPC_MESSAGE_CONTROL1(msg_class, type1) \
  msg_class##__ID,

#define IPC_MESSAGE_CONTROL2(msg_class, type1, type2) \
  msg_class##__ID,

#define IPC_MESSAGE_CONTROL3(msg_class, type1, type2, type3) \
  msg_class##__ID,

#define IPC_MESSAGE_CONTROL4(msg_class, type1, type2, type3, type4) \
  msg_class##__ID,

#define IPC_MESSAGE_CONTROL5(msg_class, type1, type2, type3, type4, type5) \
  msg_class##__ID,

#define IPC_MESSAGE_ROUTED0(msg_class) \
  msg_class##__ID,

#define IPC_MESSAGE_ROUTED1(msg_class, type1) \
  msg_class##__ID,

#define IPC_MESSAGE_ROUTED2(msg_class, type1, type2) \
  msg_class##__ID,

#define IPC_MESSAGE_ROUTED3(msg_class, type1, type2, type3) \
  msg_class##__ID,

#define IPC_MESSAGE_ROUTED4(msg_class, type1, type2, type3, type4) \
  msg_class##__ID,

#define IPC_MESSAGE_ROUTED5(msg_class, type1, type2, type3, type4, type5) \
  msg_class##__ID,

#define IPC_MESSAGE_ROUTED6(msg_class, type1, type2, type3, type4, type5, type6) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL0_0(msg_class) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL0_1(msg_class, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL0_2(msg_class, type1_out, type2_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL0_3(msg_class, type1_out, type2_out, type3_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL1_0(msg_class, type1_in) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL1_1(msg_class, type1_in, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL1_2(msg_class, type1_in, type1_out, type2_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL2_0(msg_class, type1_in, type2_in) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL2_1(msg_class, type1_in, type2_in, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_CONTROL4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED0_0(msg_class) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED0_1(msg_class, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED0_2(msg_class, type1_out, type2_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED0_3(msg_class, type1_out, type2_out, type3_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED1_0(msg_class, type1_in) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED1_1(msg_class, type1_in, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED1_2(msg_class, type1_in, type1_out, type2_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED2_0(msg_class, type1_in, type2_in) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED2_1(msg_class, type1_in, type2_in, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED3_0(msg_class, type1_in, type2_in, type3_in) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED4_0(msg_class, type1_in, type2_in, type3_in, type4_in) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  msg_class##__ID,

#define IPC_SYNC_MESSAGE_ROUTED4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  msg_class##__ID,

// Message crackers and handlers.
// Prefer to use the IPC_BEGIN_MESSAGE_MAP_EX to the older macros since they
// allow you to detect when a message could not be de-serialized. Usage:
//
//   void MyClass::OnMessageReceived(const IPC::Message& msg) {
//     bool msg_is_good = false;
//     IPC_BEGIN_MESSAGE_MAP_EX(MyClass, msg, msg_is_good)
//       IPC_MESSAGE_HANDLER(MsgClassOne, OnMsgClassOne)
//       ...more handlers here ...
//       IPC_MESSAGE_HANDLER(MsgClassTen, OnMsgClassTen)
//     IPC_END_MESSAGE_MAP_EX()
//     if (!msg_is_good) {
//       // Signal error here or terminate offending process.
//     }
//   }

#define IPC_DEFINE_MESSAGE_MAP(class_name) \
void class_name::OnMessageReceived(const IPC::Message& msg) \
  IPC_BEGIN_MESSAGE_MAP(class_name, msg)

#define IPC_BEGIN_MESSAGE_MAP_EX(class_name, msg, msg_is_ok) \
  { \
    typedef class_name _IpcMessageHandlerClass; \
    const IPC::Message& ipc_message__ = msg; \
    bool& msg_is_ok__ = msg_is_ok; \
    switch (ipc_message__.type()) { \

#define IPC_BEGIN_MESSAGE_MAP(class_name, msg) \
  { \
    typedef class_name _IpcMessageHandlerClass; \
    const IPC::Message& ipc_message__ = msg; \
    bool msg_is_ok__ = true; \
    switch (ipc_message__.type()) { \

#define IPC_MESSAGE_FORWARD(msg_class, obj, member_func) \
  case msg_class::ID: \
    msg_is_ok__ = msg_class::Dispatch(&ipc_message__, obj, &member_func); \
    break;

#define IPC_MESSAGE_HANDLER(msg_class, member_func) \
  IPC_MESSAGE_FORWARD(msg_class, this, _IpcMessageHandlerClass::member_func)

#define IPC_MESSAGE_FORWARD_DELAY_REPLY(msg_class, obj, member_func) \
    case msg_class::ID: \
    msg_class::DispatchDelayReply(&ipc_message__, obj, &member_func); \
    break;

#define IPC_MESSAGE_HANDLER_DELAY_REPLY(msg_class, member_func) \
  IPC_MESSAGE_FORWARD_DELAY_REPLY(msg_class, this, _IpcMessageHandlerClass::member_func)

#define IPC_MESSAGE_HANDLER_GENERIC(msg_class, code) \
  case msg_class::ID: \
    code; \
    break;

#define IPC_REPLY_HANDLER(func) \
   case IPC_REPLY_ID: \
     func(ipc_message__); \
     break;


#define IPC_MESSAGE_UNHANDLED(code) \
  default: \
    code; \
    break;

#define IPC_MESSAGE_UNHANDLED_ERROR() \
  IPC_MESSAGE_UNHANDLED(NOTREACHED() << \
                              "Invalid message with type = " << \
                              ipc_message__.type())

#define IPC_END_MESSAGE_MAP() \
    DCHECK(msg_is_ok__); \
  } \
}

#define IPC_END_MESSAGE_MAP_EX() \
  } \
}

#elif defined(IPC_MESSAGE_MACROS_LOG)
#undef IPC_MESSAGE_MACROS_LOG

#ifndef IPC_LOG_TABLE_CREATED
#define IPC_LOG_TABLE_CREATED
typedef void (*LogFunction)(uint16_t type,
                           std::wstring* name,
                           const IPC::Message* msg,
                           std::wstring* params);

LogFunction g_log_function_mapping[LastMsgIndex];
#endif


#define IPC_BEGIN_MESSAGES(label) \
  void label##MsgLog(uint16_t type, std::wstring* name, const IPC::Message* msg, std::wstring* params) { \
  switch (type) {

#define IPC_END_MESSAGES(label) \
     default: \
      if (name) \
        *name = L"[UNKNOWN " L ## #label L" MSG"; \
    } \
  } \
  class LoggerRegisterHelper##label { \
   public: \
    LoggerRegisterHelper##label() { \
      g_log_function_mapping[label##MsgStart] = label##MsgLog; \
    } \
  }; \
  LoggerRegisterHelper##label g_LoggerRegisterHelper##label;

#define IPC_MESSAGE_LOG(msg_class) \
     case msg_class##__ID: \
      if (name) \
        *name = L ## #msg_class; \
      if (msg && params) \
        msg_class::Log(msg, params); \
      break;

#define IPC_MESSAGE_CONTROL0(msg_class) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL1(msg_class, type1) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL2(msg_class, type1, type2) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL3(msg_class, type1, type2, type3) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL4(msg_class, type1, type2, type3, type4) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_CONTROL5(msg_class, type1, type2, type3, type4, type5) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED0(msg_class) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED1(msg_class, type1) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED2(msg_class, type1, type2) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED3(msg_class, type1, type2, type3) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED4(msg_class, type1, type2, type3, type4) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED5(msg_class, type1, type2, type3, type4, type5) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_MESSAGE_ROUTED6(msg_class, type1, type2, type3, type4, type5, type6) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_0(msg_class) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_1(msg_class, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_2(msg_class, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL0_3(msg_class, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_0(msg_class, type1_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_1(msg_class, type1_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_2(msg_class, type1_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_0(msg_class, type1_in, type2_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_1(msg_class, type1_in, type2_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_CONTROL4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_0(msg_class) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_1(msg_class, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_2(msg_class, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED0_3(msg_class, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_0(msg_class, type1_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_1(msg_class, type1_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_2(msg_class, type1_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_0(msg_class, type1_in, type2_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_1(msg_class, type1_in, type2_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_0(msg_class, type1_in, type2_in, type3_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED4_0(msg_class, type1_in, type2_in, type3_in, type4_in) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  IPC_MESSAGE_LOG(msg_class)

#define IPC_SYNC_MESSAGE_ROUTED4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  IPC_MESSAGE_LOG(msg_class)

#elif defined(IPC_MESSAGE_MACROS_CLASSES)
#undef IPC_MESSAGE_MACROS_CLASSES

#define IPC_BEGIN_MESSAGES(label)
#define IPC_END_MESSAGES(label)

#define IPC_MESSAGE_CONTROL0(msg_class) \
  class msg_class : public IPC::Message { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class() \
        : IPC::Message(MSG_ROUTING_CONTROL, \
                       ID, \
                       PRIORITY_NORMAL) {} \
  };

#define IPC_MESSAGE_CONTROL1(msg_class, type1) \
  class msg_class : public IPC::MessageWithTuple<type1> { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1& arg1) \
        : IPC::MessageWithTuple<type1>(MSG_ROUTING_CONTROL, \
                                       ID, \
                                       arg1) {} \
  };

#define IPC_MESSAGE_CONTROL2(msg_class, type1, type2) \
  class msg_class : public IPC::MessageWithTuple< Tuple2<type1, type2> > { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(const type1& arg1, const type2& arg2) \
        : IPC::MessageWithTuple< Tuple2<type1, type2> >( \
              MSG_ROUTING_CONTROL, \
              ID, \
              MakeTuple(arg1, arg2)) {} \
  };

#define IPC_MESSAGE_CONTROL3(msg_class, type1, type2, type3) \
  class msg_class : \
      public IPC::MessageWithTuple< Tuple3<type1, type2, type3> > { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(const type1& arg1, const type2& arg2, const type3& arg3) \
        : IPC::MessageWithTuple< Tuple3<type1, type2, type3> >( \
              MSG_ROUTING_CONTROL, \
              ID, \
              MakeTuple(arg1, arg2, arg3)) {} \
  };

#define IPC_MESSAGE_CONTROL4(msg_class, type1, type2, type3, type4) \
  class msg_class : \
      public IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> > { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(const type1& arg1, const type2& arg2, const type3& arg3, \
              const type4& arg4) \
        : IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> >( \
              MSG_ROUTING_CONTROL, \
              ID, \
              MakeTuple(arg1, arg2, arg3, arg4)) {} \
  };

#define IPC_MESSAGE_CONTROL5(msg_class, type1, type2, type3, type4, type5) \
  class msg_class : \
      public IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4, type5> > { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(const type1& arg1, const type2& arg2, \
              const type3& arg3, const type4& arg4, const type5& arg5) \
        : IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4, type5> >( \
            MSG_ROUTING_CONTROL, \
            ID, \
            MakeTuple(arg1, arg2, arg3, arg4, arg5)) {} \
  };

#define IPC_MESSAGE_ROUTED0(msg_class) \
  class msg_class : public IPC::Message { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(int32_t routing_id) \
        : IPC::Message(routing_id, ID, PRIORITY_NORMAL) {} \
  };

#define IPC_MESSAGE_ROUTED1(msg_class, type1) \
  class msg_class : public IPC::MessageWithTuple<type1> { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(int32_t routing_id, const type1& arg1) \
        : IPC::MessageWithTuple<type1>(routing_id, ID, arg1) {} \
  };

#define IPC_MESSAGE_ROUTED2(msg_class, type1, type2) \
  class msg_class : public IPC::MessageWithTuple< Tuple2<type1, type2> > { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(int32_t routing_id, const type1& arg1, const type2& arg2) \
        : IPC::MessageWithTuple< Tuple2<type1, type2> >( \
            routing_id, ID, MakeTuple(arg1, arg2)) {} \
  };

#define IPC_MESSAGE_ROUTED3(msg_class, type1, type2, type3) \
  class msg_class : \
      public IPC::MessageWithTuple< Tuple3<type1, type2, type3> > { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(int32_t routing_id, const type1& arg1, const type2& arg2, \
              const type3& arg3) \
        : IPC::MessageWithTuple< Tuple3<type1, type2, type3> >( \
            routing_id, ID, MakeTuple(arg1, arg2, arg3)) {} \
  };

#define IPC_MESSAGE_ROUTED4(msg_class, type1, type2, type3, type4) \
  class msg_class : \
      public IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> > { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(int32_t routing_id, const type1& arg1, const type2& arg2, \
               const type3& arg3, const type4& arg4) \
        : IPC::MessageWithTuple< Tuple4<type1, type2, type3, type4> >( \
            routing_id, ID, MakeTuple(arg1, arg2, arg3, arg4)) {} \
  };

#define IPC_MESSAGE_ROUTED5(msg_class, type1, type2, type3, type4, type5) \
  class msg_class : \
      public IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4, type5> > { \
   public: \
    enum { ID = msg_class##__ID }; \
    msg_class(int32_t routing_id, const type1& arg1, const type2& arg2, \
              const type3& arg3, const type4& arg4, const type5& arg5) \
        : IPC::MessageWithTuple< Tuple5<type1, type2, type3, type4, type5> >( \
            routing_id, ID, MakeTuple(arg1, arg2, arg3, arg4, arg5)) {} \
  };

#define IPC_MESSAGE_ROUTED6(msg_class, type1, type2, type3, type4, type5, \
                            type6)                                      \
  class msg_class :                                                     \
      public IPC::MessageWithTuple< Tuple6<type1, type2, type3, type4, type5, \
                                           type6> > {                   \
 public:                                                                \
    enum { ID = msg_class##__ID };                                      \
    msg_class(int32_t routing_id, const type1& arg1, const type2& arg2,   \
              const type3& arg3, const type4& arg4, const type5& arg5,  \
              const type6& arg6)                                        \
      : IPC::MessageWithTuple< Tuple6<type1, type2, type3, type4, type5, \
      type6> >(                                                         \
          routing_id, ID, MakeTuple(arg1, arg2, arg3, arg4, arg5, arg6)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL0_0(msg_class) \
  class msg_class : public IPC::MessageWithReply<Tuple0, Tuple0 > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class() \
        : IPC::MessageWithReply<Tuple0, Tuple0 >( \
            MSG_ROUTING_CONTROL, ID, \
            MakeTuple(), MakeTuple()) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL0_1(msg_class, type1_out) \
  class msg_class : public IPC::MessageWithReply<Tuple0, Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(type1_out* arg1) \
        : IPC::MessageWithReply<Tuple0, Tuple1<type1_out&> >( \
            MSG_ROUTING_CONTROL, \
            ID, \
            MakeTuple(), MakeRefTuple(*arg1)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL0_2(msg_class, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(type1_out* arg1, type2_out* arg2) \
        : IPC::MessageWithReply<Tuple0, Tuple2<type1_out&, type2_out&> >( \
            MSG_ROUTING_CONTROL, \
            ID, \
            MakeTuple(), MakeRefTuple(*arg1, *arg2)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL0_3(msg_class, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, \
          Tuple3<type1_out&, type2_out&, type3_out&> >{ \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(type1_out* arg1, type2_out* arg2, type3_out* arg3) \
        : IPC::MessageWithReply<Tuple0,  \
            Tuple3<type1_out&, type2_out&, type3_out&> >(MSG_ROUTING_CONTROL, \
            ID, \
            MakeTuple(), MakeRefTuple(*arg1, *arg2, *arg3)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL1_0(msg_class, type1_in) \
  class msg_class : \
      public IPC::MessageWithReply<type1_in, Tuple0 > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1) \
        : IPC::MessageWithReply<type1_in, Tuple0 >( \
            MSG_ROUTING_CONTROL, ID, \
            arg1, MakeTuple()) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL1_1(msg_class, type1_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<type1_in, Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, type1_out* arg2) \
        : IPC::MessageWithReply<type1_in, Tuple1<type1_out&> >( \
            MSG_ROUTING_CONTROL, ID, \
            arg1, MakeRefTuple(*arg2)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL1_2(msg_class, type1_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<type1_in, Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, type1_out* arg2, type2_out* arg3) \
        : IPC::MessageWithReply<type1_in, Tuple2<type1_out&, type2_out&> >( \
            MSG_ROUTING_CONTROL, ID, \
            arg1, MakeRefTuple(*arg2, *arg3)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<type1_in, \
          Tuple3<type1_out&, type2_out&, type3_out&> >{ \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, type1_out* arg2, type2_out* arg3, type3_out* arg4) \
        : IPC::MessageWithReply<type1_in, \
            Tuple3<type1_out&, type2_out&, type3_out&> >(MSG_ROUTING_CONTROL, \
            ID, \
            arg1, MakeRefTuple(*arg2, *arg3, *arg4)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL2_0(msg_class, type1_in, type2_in) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple0 > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, const type2_in& arg2) \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple0 >( \
            MSG_ROUTING_CONTROL, ID, \
            MakeTuple(arg1, arg2), MakeTuple()) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL2_1(msg_class, type1_in, type2_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, type1_out* arg3) \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple1<type1_out&> >( \
            MSG_ROUTING_CONTROL, ID, \
            MakeTuple(arg1, arg2), MakeRefTuple(*arg3)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4) \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
            Tuple2<type1_out&, type2_out&> >(MSG_ROUTING_CONTROL, ID, \
            MakeTuple(arg1, arg2), MakeRefTuple(*arg3, *arg4)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4, type3_out* arg5) \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
            Tuple3<type1_out&, type2_out&, type3_out&> >(MSG_ROUTING_CONTROL, \
            ID, \
            MakeTuple(arg1, arg2), MakeRefTuple(*arg3, *arg4, *arg5)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4) \
        : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
            Tuple1<type1_out&> >(MSG_ROUTING_CONTROL, ID, \
            MakeTuple(arg1, arg2, arg3), MakeRefTuple(*arg4)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5) \
        : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
            Tuple2<type1_out&, type2_out&> >(MSG_ROUTING_CONTROL, ID, \
            MakeTuple(arg1, arg2, arg3), MakeRefTuple(*arg4, *arg5)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5, type3_out* arg6) \
        : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
            Tuple3<type1_out&, type2_out&, type3_out&> >(MSG_ROUTING_CONTROL, \
            ID, \
            MakeTuple(arg1, arg2, arg3), MakeRefTuple(*arg4, *arg5, *arg6)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg6) \
        : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
            Tuple1<type1_out&> >(MSG_ROUTING_CONTROL, ID, \
            MakeTuple(arg1, arg2, arg3, arg4), MakeRefTuple(*arg6)) {} \
  };

#define IPC_SYNC_MESSAGE_CONTROL4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg5, type2_out* arg6) \
        : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
            Tuple2<type1_out&, type2_out&> >(MSG_ROUTING_CONTROL, ID, \
            MakeTuple(arg1, arg2, arg3, arg4), MakeRefTuple(*arg5, *arg6)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED0_1(msg_class, type1_out) \
  class msg_class : public IPC::MessageWithReply<Tuple0, Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, type1_out* arg1) \
        : IPC::MessageWithReply<Tuple0, Tuple1<type1_out&> >( \
            routing_id, ID, \
            MakeTuple(), MakeRefTuple(*arg1)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED0_0(msg_class) \
  class msg_class : public IPC::MessageWithReply<Tuple0, Tuple0 > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id) \
        : IPC::MessageWithReply<Tuple0, Tuple0 >( \
            routing_id, ID, \
            MakeTuple(), MakeTuple()) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED0_2(msg_class, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, type1_out* arg1, type2_out* arg2) \
        : IPC::MessageWithReply<Tuple0, Tuple2<type1_out&, type2_out&> >( \
            routing_id, ID, \
            MakeTuple(), MakeRefTuple(*arg1, *arg2)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED0_3(msg_class, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple0, \
          Tuple3<type1_out&, type2_out&, type3_out&> >{ \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, type1_out* arg1, type2_out* arg2, type3_out* arg3) \
        : IPC::MessageWithReply<Tuple0,  \
            Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID, \
            MakeTuple(), MakeRefTuple(*arg1, *arg2, *arg3)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED1_0(msg_class, type1_in) \
  class msg_class : \
      public IPC::MessageWithReply<type1_in, Tuple0 > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1) \
        : IPC::MessageWithReply<type1_in, Tuple0 >( \
            routing_id, ID, \
            arg1, MakeTuple()) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED1_1(msg_class, type1_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<type1_in, Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, type1_out* arg2) \
        : IPC::MessageWithReply<type1_in, Tuple1<type1_out&> >( \
            routing_id, ID, \
            arg1, MakeRefTuple(*arg2)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED1_2(msg_class, type1_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<type1_in, Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, type1_out* arg2, type2_out* arg3) \
        : IPC::MessageWithReply<type1_in, Tuple2<type1_out&, type2_out&> >( \
            routing_id, ID, \
            arg1, MakeRefTuple(*arg2, *arg3)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<type1_in, \
          Tuple3<type1_out&, type2_out&, type3_out&> >{ \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, type1_out* arg2, type2_out* arg3, type3_out* arg4) \
        : IPC::MessageWithReply<type1_in, \
            Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID, \
            arg1, MakeRefTuple(*arg2, *arg3, *arg4)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out) \
  class msg_class : \
      public IPC::MessageWithReply<type1_in, \
          Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> >{ \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, type1_out* arg2, type2_out* arg3, type3_out* arg4, type4_out* arg5) \
        : IPC::MessageWithReply<type1_in, \
            Tuple4<type1_out&, type2_out&, type3_out&, type4_out&> >(routing_id, ID, \
            arg1, MakeRefTuple(*arg2, *arg3, *arg4, *arg5)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED2_0(msg_class, type1_in, type2_in) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple0 > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2) \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple0 >( \
            routing_id, ID, \
            MakeTuple(arg1, arg2), MakeTuple()) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED2_1(msg_class, type1_in, type2_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, type1_out* arg3) \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, Tuple1<type1_out&> >( \
            routing_id, ID, \
            MakeTuple(arg1, arg2), MakeRefTuple(*arg3)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4) \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
            Tuple2<type1_out&, type2_out&> >(routing_id, ID, \
            MakeTuple(arg1, arg2), MakeRefTuple(*arg3, *arg4)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, type1_out* arg3, type2_out* arg4, type3_out* arg5) \
        : IPC::MessageWithReply<Tuple2<type1_in, type2_in>, \
            Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID, \
            MakeTuple(arg1, arg2), MakeRefTuple(*arg3, *arg4, *arg5)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED3_0(msg_class, type1_in, type2_in, type3_in) \
    class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, Tuple0 > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3) \
        : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, Tuple0>( \
            routing_id, ID, \
            MakeTuple(arg1, arg2, arg3), MakeTuple()) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4) \
        : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
            Tuple1<type1_out&> >(routing_id, ID, \
            MakeTuple(arg1, arg2, arg3), MakeRefTuple(*arg4)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5) \
        : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
            Tuple2<type1_out&, type2_out&> >(routing_id, ID, \
            MakeTuple(arg1, arg2, arg3), MakeRefTuple(*arg4, *arg5)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
          Tuple3<type1_out&, type2_out&, type3_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, type1_out* arg4, type2_out* arg5, type3_out* arg6) \
        : IPC::MessageWithReply<Tuple3<type1_in, type2_in, type3_in>, \
            Tuple3<type1_out&, type2_out&, type3_out&> >(routing_id, ID, \
            MakeTuple(arg1, arg2, arg3), MakeRefTuple(*arg4, *arg5, *arg6)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED4_0(msg_class, type1_in, type2_in, type3_in, type4_in) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple0 > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4) \
        : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
            Tuple0 >(routing_id, ID, \
            MakeTuple(arg1, arg2, arg3, arg4), MakeTuple()) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple1<type1_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg6) \
        : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
            Tuple1<type1_out&> >(routing_id, ID, \
            MakeTuple(arg1, arg2, arg3, arg4), MakeRefTuple(*arg6)) {} \
  };

#define IPC_SYNC_MESSAGE_ROUTED4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  class msg_class : \
      public IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
          Tuple2<type1_out&, type2_out&> > { \
   public: \
   enum { ID = msg_class##__ID }; \
    msg_class(int routing_id, const type1_in& arg1, const type2_in& arg2, const type3_in& arg3, const type4_in& arg4, type1_out* arg5, type2_out* arg6) \
        : IPC::MessageWithReply<Tuple4<type1_in, type2_in, type3_in, type4_in>, \
            Tuple2<type1_out&, type2_out&> >(routing_id, ID, \
            MakeTuple(arg1, arg2, arg3, arg4), MakeRefTuple(*arg5, *arg6)) {} \
  };

#endif  // #if defined()
