/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _RUSTSDPGLUE_H_
#define _RUSTSDPGLUE_H_

#include <string>
#include <vector>
#include <utility>
#include "sdp/Sdp.h"
#include "sdp/RsdparsaSdpInc.h"

namespace mozilla {

struct FreeRustSdpSession {
  void operator()(RustSdpSession* aSess) { sdp_free_session(aSess); }
};

typedef UniquePtr<RustSdpSession, FreeRustSdpSession> RsdparsaSessionHandle;

std::string convertStringView(StringView str);
std::vector<std::string> convertStringVec(StringVec* vec);
std::string convertAddress(RustAddress* address);
std::pair<sdp::AddrType, std::string> convertExplicitlyTypedAddress(
    RustExplicitlyTypedAddress* addr);
std::vector<uint8_t> convertU8Vec(U8Vec* vec);
std::vector<uint16_t> convertU16Vec(U16Vec* vec);
std::vector<uint32_t> convertU32Vec(U32Vec* vec);
std::vector<float> convertF32Vec(F32Vec* vec);

}  // namespace mozilla

#endif
