/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://w3c.github.io/webrtc-pc/#rtcrtptransceiver-interface
 */

enum RTCRtpTransceiverDirection {
    "sendrecv",
    "sendonly",
    "recvonly",
    "inactive"
};

dictionary RTCRtpTransceiverInit {
    RTCRtpTransceiverDirection         direction = "sendrecv";
    sequence<MediaStream>              streams = [];
    // TODO: bug 1396918
    // sequence<RTCRtpEncodingParameters> sendEncodings;
};

[Pref="media.peerconnection.enabled",
 Exposed=Window]
interface RTCRtpTransceiver {
    readonly attribute DOMString?                  mid;
    [SameObject]
    readonly attribute RTCRtpSender                sender;
    [SameObject]
    readonly attribute RTCRtpReceiver              receiver;
    readonly attribute boolean                     stopped;
    [SetterThrows]
             attribute RTCRtpTransceiverDirection  direction;
    readonly attribute RTCRtpTransceiverDirection? currentDirection;

    [Throws]
    void stop();
    // TODO: bug 1396922
    // void setCodecPreferences(sequence<RTCRtpCodecCapability> codecs);

    [ChromeOnly]
    void setAddTrackMagic();
    [ChromeOnly]
    void setDirectionInternal(RTCRtpTransceiverDirection direction);

    [ChromeOnly]
    DOMString getKind();
    [ChromeOnly]
    boolean hasBeenUsedToSend();
};

