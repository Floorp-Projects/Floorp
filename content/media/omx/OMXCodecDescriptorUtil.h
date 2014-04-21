/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OMXCodecDescriptorUtil_h_
#define OMXCodecDescriptorUtil_h_

#include <stagefright/foundation/AMessage.h>
#include <stagefright/MediaErrors.h>

#include <nsTArray.h>

#include "OMXCodecWrapper.h"

namespace android {
// Generate decoder config blob using aConfigData provided by encoder.
// The output will be stored in aOutputBuf.
// aFormat specifies the output format: AVC_MP4 is for MP4 file, and AVC_NAL is
// for RTP packet used by WebRTC.
status_t GenerateAVCDescriptorBlob(sp<AMessage>& aConfigData,
                                   nsTArray<uint8_t>* aOutputBuf,
                                   OMXVideoEncoder::BlobFormat aFormat);

}

#endif // OMXCodecDescriptorUtil_h_
