/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://lists.w3.org/Archives/Public/public-webrtc/2014May/0067.html
 */

[Pref="media.peerconnection.enabled",
 Exposed=Window]
interface RTCRtpReceiver {
  readonly attribute MediaStreamTrack   track;
  readonly attribute RTCDtlsTransport?  transport;
  Promise<RTCStatsReport>               getStats();
  [Pref="media.peerconnection.rtpsourcesapi.enabled"]
  sequence<RTCRtpContributingSource>    getContributingSources();
  [Pref="media.peerconnection.rtpsourcesapi.enabled"]
  sequence<RTCRtpSynchronizationSource> getSynchronizationSources();

  // test-only: for testing getContributingSources
  [ChromeOnly]
  void mozInsertAudioLevelForContributingSource(unsigned long source,
                                                DOMHighResTimeStamp timestamp,
                                                unsigned long rtpTimestamp,
                                                boolean hasLevel,
                                                byte level);
};
