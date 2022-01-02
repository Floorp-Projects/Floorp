// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mach_ipc_mac.h"

#include "base/logging.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsDebug.h"

namespace {
// Struct for sending a Mach message with a single port.
struct MachSinglePortMessage {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t data;
};

// Struct for receiving a Mach message with a single port.
struct MachSinglePortMessageTrailer : MachSinglePortMessage {
  mach_msg_audit_trailer_t trailer;
};
}  // namespace

//==============================================================================
kern_return_t MachSendPortSendRight(
    mach_port_t endpoint, mach_port_t attachment,
    mozilla::Maybe<mach_msg_timeout_t> opt_timeout,
    mach_msg_type_name_t endpoint_disposition) {
  mach_msg_option_t opts = MACH_SEND_MSG;
  mach_msg_timeout_t timeout = MACH_MSG_TIMEOUT_NONE;
  if (opt_timeout) {
    opts |= MACH_SEND_TIMEOUT;
    timeout = *opt_timeout;
  }

  MachSinglePortMessage send_msg{};
  send_msg.header.msgh_bits =
      MACH_MSGH_BITS(endpoint_disposition, 0) | MACH_MSGH_BITS_COMPLEX;
  send_msg.header.msgh_size = sizeof(send_msg);
  send_msg.header.msgh_remote_port = endpoint;
  send_msg.header.msgh_local_port = MACH_PORT_NULL;
  send_msg.header.msgh_reserved = 0;
  send_msg.header.msgh_id = 0;
  send_msg.body.msgh_descriptor_count = 1;
  send_msg.data.name = attachment;
  send_msg.data.disposition = MACH_MSG_TYPE_COPY_SEND;
  send_msg.data.type = MACH_MSG_PORT_DESCRIPTOR;

  return mach_msg(&send_msg.header, opts, send_msg.header.msgh_size, 0,
                  MACH_PORT_NULL, timeout, MACH_PORT_NULL);
}

//==============================================================================
kern_return_t MachReceivePortSendRight(
    const mozilla::UniqueMachReceiveRight& endpoint,
    mozilla::Maybe<mach_msg_timeout_t> opt_timeout,
    mozilla::UniqueMachSendRight* attachment, audit_token_t* audit_token) {
  mach_msg_option_t opts = MACH_RCV_MSG |
                           MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
                           MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);
  mach_msg_timeout_t timeout = MACH_MSG_TIMEOUT_NONE;
  if (opt_timeout) {
    opts |= MACH_RCV_TIMEOUT;
    timeout = *opt_timeout;
  }

  MachSinglePortMessageTrailer recv_msg{};
  recv_msg.header.msgh_local_port = endpoint.get();
  recv_msg.header.msgh_size = sizeof(recv_msg);

  kern_return_t kr =
      mach_msg(&recv_msg.header, opts, 0, recv_msg.header.msgh_size,
               endpoint.get(), timeout, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    return kr;
  }

  if (NS_WARN_IF(!(recv_msg.header.msgh_bits & MACH_MSGH_BITS_COMPLEX)) ||
      NS_WARN_IF(recv_msg.body.msgh_descriptor_count != 1) ||
      NS_WARN_IF(recv_msg.data.type != MACH_MSG_PORT_DESCRIPTOR) ||
      NS_WARN_IF(recv_msg.data.disposition != MACH_MSG_TYPE_MOVE_SEND) ||
      NS_WARN_IF(recv_msg.header.msgh_size != sizeof(MachSinglePortMessage))) {
    mach_msg_destroy(&recv_msg.header);
    return KERN_FAILURE;  // Invalid message format
  }

  attachment->reset(recv_msg.data.name);
  if (audit_token) {
    *audit_token = recv_msg.trailer.msgh_audit;
  }
  return KERN_SUCCESS;
}
