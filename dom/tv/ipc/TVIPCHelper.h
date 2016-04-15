/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVIPCHelper_h
#define mozilla_dom_TVIPCHelper_h

#include "mozilla/dom/PTVTypes.h"
#include "nsITVService.h"

namespace mozilla {
namespace dom {

/*
 * TVTunerData Pack/Unpack.
 */
void
PackTVIPCTunerData(nsITVTunerData* aTunerData, TVIPCTunerData& aIPCTunerData);

void
UnpackTVIPCTunerData(nsITVTunerData* aTunerData,
                     const TVIPCTunerData& aIPCTunerData);

/*
 * TVChannelData Pack/Unpack.
 */
void
PackTVIPCChannelData(nsITVChannelData* aChannelData,
                     TVIPCChannelData& aIPCChannelData);

void
UnpackTVIPCChannelData(nsITVChannelData* aChannelData,
                       const TVIPCChannelData& aIPCChannelData);

/*
 * TVProgramData Pack/Unpack.
 */
void
PackTVIPCProgramData(nsITVProgramData* aProgramData,
                     TVIPCProgramData& aIPCProgramData);

void
UnpackTVIPCProgramData(nsITVProgramData* aProgramData,
                       const TVIPCProgramData& aIPCProgramData);

/**
 * nsITVGonkNativeHandleData Serialize/De-serialize.
 */
void
PackTVIPCGonkNativeHandleData(nsITVGonkNativeHandleData* aNativeHandleData,
                              TVIPCGonkNativeHandleData& aIPCNativeHandleData);

void
UnpackTVIPCGonkNativeHandleData(
  nsITVGonkNativeHandleData* aNativeHandleData,
  const TVIPCGonkNativeHandleData& aIPCNativeHandleData);

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVIPCHelper_h
