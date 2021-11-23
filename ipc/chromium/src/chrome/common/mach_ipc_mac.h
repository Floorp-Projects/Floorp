/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MACH_IPC_MAC_H_
#define BASE_MACH_IPC_MAC_H_

#include <mach/mach.h>
#include <mach/message.h>
#include <servers/bootstrap.h>
#include <sys/types.h>

#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/UniquePtrExtensions.h"

//==============================================================================
// Helper function for sending a minimal mach IPC messages with a single send
// right attached. The endpoint will not be consumed unless the
// `endpoint_disposition` argument is set to a consuming disposition, and
// `KERN_SUCCESS` is returned.
kern_return_t MachSendPortSendRight(
    mach_port_t endpoint, mach_port_t attachment,
    mozilla::Maybe<mach_msg_timeout_t> opt_timeout,
    mach_msg_type_name_t endpoint_disposition = MACH_MSG_TYPE_COPY_SEND);

//==============================================================================
// Helper function for receiving a minimal mach IPC message with a single send
// right attached.
// If the `audit_token` parameter is provided, it will be populated with the
// sender's audit token, which can be used to verify the identity of the sender.
kern_return_t MachReceivePortSendRight(
    const mozilla::UniqueMachReceiveRight& endpoint,
    mozilla::Maybe<mach_msg_timeout_t> opt_timeout,
    mozilla::UniqueMachSendRight* attachment,
    audit_token_t* audit_token = nullptr);

#endif  // BASE_MACH_IPC_MAC_H_
