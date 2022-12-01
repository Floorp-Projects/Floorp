/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVIDEOCODEC_H_
#define GMPVIDEOCODEC_H_

#include <string>

#include "PerformanceRecorder.h"

namespace mozilla {

class WebrtcVideoDecoder;
class WebrtcVideoEncoder;

class GmpVideoCodec {
 public:
  static WebrtcVideoEncoder* CreateEncoder(
      const webrtc::SdpVideoFormat& aFormat, std::string aPCHandle);
  static WebrtcVideoDecoder* CreateDecoder(std::string aPCHandle,
                                           TrackingId aTrackingId);
};

}  // namespace mozilla

#endif
