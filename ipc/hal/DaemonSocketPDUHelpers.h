/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DaemonSocketPDUHelpers_h
#define mozilla_ipc_DaemonSocketPDUHelpers_h

#include <stdint.h>

namespace mozilla {
namespace ipc {

struct DaemonSocketPDUHeader {
  DaemonSocketPDUHeader()
    : mService(0x00)
    , mOpcode(0x00)
    , mLength(0x00)
  { }

  DaemonSocketPDUHeader(uint8_t aService, uint8_t aOpcode, uint16_t aLength)
    : mService(aService)
    , mOpcode(aOpcode)
    , mLength(aLength)
  { }

  uint8_t mService;
  uint8_t mOpcode;
  uint16_t mLength;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_DaemonSocketPDUHelpers_h
