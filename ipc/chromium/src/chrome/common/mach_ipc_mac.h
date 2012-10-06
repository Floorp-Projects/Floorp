// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MACH_IPC_MAC_H_
#define BASE_MACH_IPC_MAC_H_

#include <mach/mach.h>
#include <mach/message.h>
#include <servers/bootstrap.h>
#include <sys/types.h>

#include <CoreServices/CoreServices.h>

#include "base/basictypes.h"

//==============================================================================
// DISCUSSION:
//
// The three main classes of interest are
//
//  MachMessage:    a wrapper for a mach message of the following form
//   mach_msg_header_t
//   mach_msg_body_t
//   optional descriptors
//   optional extra message data
//
//  MachReceiveMessage and MachSendMessage subclass MachMessage
//    and are used instead of MachMessage which is an abstract base class
//
//  ReceivePort:
//    Represents a mach port for which we have receive rights
//
//  MachPortSender:
//    Represents a mach port for which we have send rights
//
// Here's an example to receive a message on a server port:
//
//        // This creates our named server port
//        ReceivePort receivePort("com.Google.MyService");
//
//        MachReceiveMessage message;
//        kern_return_t result = receivePort.WaitForMessage(&message, 0);
//
//        if (result == KERN_SUCCESS && message.GetMessageID() == 57) {
//          mach_port_t task = message.GetTranslatedPort(0);
//          mach_port_t thread = message.GetTranslatedPort(1);
//
//          char *messageString = message.GetData();
//
//          printf("message string = %s\n", messageString);
//        }
//
// Here is an example of using these classes to send a message to this port:
//
//    // send to already named port
//    MachPortSender sender("com.Google.MyService");
//    MachSendMessage message(57);      // our message ID is 57
//
//    // add some ports to be translated for us
//    message.AddDescriptor(mach_task_self());     // our task
//    message.AddDescriptor(mach_thread_self());   // this thread
//
//    char messageString[] = "Hello server!\n";
//    message.SetData(messageString, strlen(messageString)+1);
//    // timeout 1000ms
//    kern_return_t result = sender.SendMessage(message, 1000);
//

#define PRINT_MACH_RESULT(result_, message_) \
  printf(message_" %s (%d)\n", mach_error_string(result_), result_ );

//==============================================================================
// A wrapper class for mach_msg_port_descriptor_t (with same memory layout)
// with convenient constructors and accessors
class MachMsgPortDescriptor : public mach_msg_port_descriptor_t {
 public:
  // General-purpose constructor
  MachMsgPortDescriptor(mach_port_t in_name,
                        mach_msg_type_name_t in_disposition) {
    name = in_name;
    pad1 = 0;
    pad2 = 0;
    disposition = in_disposition;
    type = MACH_MSG_PORT_DESCRIPTOR;
  }

  // For passing send rights to a port
  MachMsgPortDescriptor(mach_port_t in_name) {
    name = in_name;
    pad1 = 0;
    pad2 = 0;
    disposition = MACH_MSG_TYPE_PORT_SEND;
    type = MACH_MSG_PORT_DESCRIPTOR;
  }

  // Copy constructor
  MachMsgPortDescriptor(const MachMsgPortDescriptor& desc) {
    name = desc.name;
    pad1 = desc.pad1;
    pad2 = desc.pad2;
    disposition = desc.disposition;
    type = desc.type;
  }

  mach_port_t GetMachPort() const {
    return name;
  }

  mach_msg_type_name_t GetDisposition() const {
    return disposition;
  }

  // For convenience
  operator mach_port_t() const {
    return GetMachPort();
  }
};

//==============================================================================
// MachMessage: a wrapper for a mach message
//  (mach_msg_header_t, mach_msg_body_t, extra data)
//
//  This considerably simplifies the construction of a message for sending
//  and the getting at relevant data and descriptors for the receiver.
//
//  This class can be initialized using external storage of an arbitrary size
//  or it can manage storage internally.
//  1. If storage is allocated internally, the combined size of the descriptors
//  plus data must be less than 1024.  But as a benefit no memory allocation is
//  necessary.
//  2. For external storage, a buffer of at least EmptyMessageSize() must be
//  provided.
//
//  A MachMessage object is used by ReceivePort::WaitForMessage
//  and MachPortSender::SendMessage
//
class MachMessage {
 public:

  virtual ~MachMessage();

  // The receiver of the message can retrieve the raw data this way
  u_int8_t *GetData() {
    return GetDataLength() > 0 ? GetDataPacket()->data : NULL;
  }

  u_int32_t GetDataLength() {
    return EndianU32_LtoN(GetDataPacket()->data_length);
  }

  // The message ID may be used as a code identifying the type of message
  void SetMessageID(int32_t message_id) {
    GetDataPacket()->id = EndianU32_NtoL(message_id);
  }

  int32_t GetMessageID() { return EndianU32_LtoN(GetDataPacket()->id); }

  // Adds a descriptor (typically a mach port) to be translated
  // returns true if successful, otherwise not enough space
  bool AddDescriptor(const MachMsgPortDescriptor &desc);

  int GetDescriptorCount() const {
                                   return storage_->body.msgh_descriptor_count;
                                 }
  MachMsgPortDescriptor *GetDescriptor(int n);

  // Convenience method which gets the mach port described by the descriptor
  mach_port_t GetTranslatedPort(int n);

  // A simple message is one with no descriptors
  bool IsSimpleMessage() const { return GetDescriptorCount() == 0; }

  // Sets raw data for the message (returns false if not enough space)
  bool SetData(const void* data, int32_t data_length);

 protected:
  // Consider this an abstract base class - must create an actual instance
  // of MachReceiveMessage or MachSendMessage
  MachMessage();

  // Constructor for use with preallocate storage.
  // storage_length must be >= EmptyMessageSize()
  MachMessage(void *storage, size_t storage_length);

  friend class ReceivePort;
  friend class MachPortSender;

  // Represents raw data in our message
  struct MessageDataPacket {
    int32_t  id;          // little-endian
    int32_t  data_length; // little-endian
    u_int8_t data[1];     // actual size limited by storage_length_bytes_
  };

  MessageDataPacket* GetDataPacket();

  void SetDescriptorCount(int n);
  void SetDescriptor(int n, const MachMsgPortDescriptor &desc);

  // Returns total message size setting msgh_size in the header to this value
  int CalculateSize();

  // Returns total storage size that this object can grow to, this is inclusive
  // of the mach header.
  size_t MaxSize() const { return storage_length_bytes_; }

 protected:
  mach_msg_header_t *Head() { return &(storage_->head); }

 private:
  struct MachMessageData {
    mach_msg_header_t  head;
    mach_msg_body_t    body;
    // descriptors and data may be embedded here.
    u_int8_t           padding[1024];
  };

 // kEmptyMessageSize needs to have the definition of MachMessageData before
 // it.
 public:
   // The size of an empty message with no data.
  static const size_t kEmptyMessageSize = sizeof(mach_msg_header_t) +
                                          sizeof(mach_msg_body_t) +
                                          sizeof(MessageDataPacket);

 private:
  MachMessageData *storage_;
  size_t storage_length_bytes_;
  bool own_storage_;  // Is storage owned by this object?
};

//==============================================================================
// MachReceiveMessage and MachSendMessage are useful to separate the idea
// of a mach message being sent and being received, and adds increased type
// safety:
//  ReceivePort::WaitForMessage() only accepts a MachReceiveMessage
//  MachPortSender::SendMessage() only accepts a MachSendMessage

//==============================================================================
class MachReceiveMessage : public MachMessage {
 public:
  MachReceiveMessage() : MachMessage() {}
  MachReceiveMessage(void *storage, size_t storage_length)
      : MachMessage(storage, storage_length) {}

 private:
    DISALLOW_COPY_AND_ASSIGN(MachReceiveMessage);
};

//==============================================================================
class MachSendMessage : public MachMessage {
 public:
  MachSendMessage(int32_t message_id);
  MachSendMessage(void *storage, size_t storage_length, int32_t message_id);

 private:
  void Initialize(int32_t message_id);

  DISALLOW_COPY_AND_ASSIGN(MachSendMessage);
};

//==============================================================================
// Represents a mach port for which we have receive rights
class ReceivePort {
 public:
  // Creates a new mach port for receiving messages and registers a name for it
  ReceivePort(const char *receive_port_name);

  // Given an already existing mach port, use it.  We take ownership of the
  // port and deallocate it in our destructor.
  ReceivePort(mach_port_t receive_port);

  // Create a new mach port for receiving messages
  ReceivePort();

  ~ReceivePort();

  // Waits on the mach port until message received or timeout
  kern_return_t WaitForMessage(MachReceiveMessage *out_message,
                               mach_msg_timeout_t timeout);

  // The underlying mach port that we wrap
  mach_port_t  GetPort() const { return port_; }

 private:
  mach_port_t   port_;
  kern_return_t init_result_;

  DISALLOW_COPY_AND_ASSIGN(ReceivePort);
};

//==============================================================================
// Represents a mach port for which we have send rights
class MachPortSender {
 public:
  // get a port with send rights corresponding to a named registered service
  MachPortSender(const char *receive_port_name);


  // Given an already existing mach port, use it.
  MachPortSender(mach_port_t send_port);

  kern_return_t SendMessage(MachSendMessage &message,
                            mach_msg_timeout_t timeout);

 private:
  mach_port_t   send_port_;
  kern_return_t init_result_;

  DISALLOW_COPY_AND_ASSIGN(MachPortSender);
};

#endif // BASE_MACH_IPC_MAC_H_
