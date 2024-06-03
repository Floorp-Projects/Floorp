/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://w3c.github.io/webrtc-pc/#rtcrtpsender-interface
 */

enum RTCPriorityType {
  "very-low",
  "low",
  "medium",
  "high"
};

enum RTCDegradationPreference {
  "maintain-framerate",
  "maintain-resolution",
  "balanced"
};

dictionary RTCRtpEncodingParameters {
  boolean                  active = true;
  // From https://www.w3.org/TR/webrtc-priority/
  RTCPriorityType          priority = "low";
  unsigned long            maxBitrate;
  DOMString                rid;
  double                   scaleResolutionDownBy;
  // From https://w3c.github.io/webrtc-extensions/#rtcrtpencodingparameters-dictionary
  double                   maxFramerate;
};

dictionary RTCRtpHeaderExtensionParameters {
  DOMString      uri;
  unsigned short id;
  boolean        encrypted;
};

dictionary RTCRtcpParameters {
  DOMString cname;
  boolean   reducedSize;
};

[GenerateEqualityOperator]
dictionary RTCRtpCodecParameters : RTCRtpCodec {
  required octet payloadType;
};

dictionary RTCRtpParameters {
  // We do not support these, but every wpt test involving parameters insists
  // that these be present, regardless of whether the test-case has anything to
  // do with these in particular (see validateRtpParameters).
  sequence<RTCRtpHeaderExtensionParameters> headerExtensions;
  RTCRtcpParameters                         rtcp;
  // Don't make this required just yet; there are still implementations out
  // there that build RTCRtpParameters themselves
  sequence<RTCRtpCodecParameters>           codecs;
};

dictionary RTCRtpSendParameters : RTCRtpParameters {
  DOMString transactionId;
  required sequence<RTCRtpEncodingParameters> encodings;
};

dictionary RTCRtpReceiveParameters : RTCRtpParameters {
};
