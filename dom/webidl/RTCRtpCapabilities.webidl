/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webrtc-pc/#dom-rtcrtpcapabilities
 */

dictionary RTCRtpCapabilities {
  required sequence<RTCRtpCodecCapability> codecs;
  required sequence<RTCRtpHeaderExtensionCapability> headerExtensions;
};

dictionary RTCRtpCodecCapability : RTCRtpCodec {
};

dictionary RTCRtpCodec {
  required DOMString mimeType;
  required unsigned long clockRate;
  unsigned short channels;
  DOMString sdpFmtpLine;
};

dictionary RTCRtpHeaderExtensionCapability {
  required DOMString uri;
};
