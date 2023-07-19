/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://w3c.github.io/webrtc-pc/#rtcrtpsender-interface
 */

[Pref="media.peerconnection.enabled",
 Exposed=Window]
interface RTCRtpSender {
  readonly attribute MediaStreamTrack? track;
  readonly attribute RTCDtlsTransport? transport;
  static RTCRtpCapabilities? getCapabilities(DOMString kind);
  [NewObject]
  Promise<undefined> setParameters (RTCRtpSendParameters parameters);
  RTCRtpSendParameters getParameters();
  [Throws]
  Promise<undefined> replaceTrack(MediaStreamTrack? withTrack);
  [Throws]
  undefined setStreams(MediaStream... streams);
  [NewObject]
  Promise<RTCStatsReport> getStats();
  readonly attribute RTCDTMFSender? dtmf;
  [ChromeOnly]
  sequence<MediaStream> getStreams();
  [ChromeOnly]
  undefined setStreamsImpl(MediaStream... streams);
  [ChromeOnly]
  undefined setTrack(MediaStreamTrack? track);
};

// https://w3c.github.io/webrtc-encoded-transform/#specification
partial interface RTCRtpSender {
  [SetterThrows,
   Pref="media.peerconnection.scripttransform.enabled"] attribute RTCRtpTransform? transform;
};
