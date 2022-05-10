/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef webrtc_tcp_socket_log_h__
#define webrtc_tcp_socket_log_h__

#include "mozilla/Logging.h"

namespace mozilla::net {
extern LazyLogModule webrtcTCPSocketLog;
}  // namespace mozilla::net

#undef LOG
#define LOG(args) \
  MOZ_LOG(mozilla::net::webrtcTCPSocketLog, mozilla::LogLevel::Debug, args)

#endif  // webrtc_tcp_socket_log_h__
