/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webrtc-encoded-transform
 */

dictionary RTCEncodedAudioFrameMetadata {
    unsigned long synchronizationSource;
    octet payloadType;
    sequence<unsigned long> contributingSources;
    short sequenceNumber;
};

[Pref="media.peerconnection.enabled",
 Pref="media.peerconnection.scripttransform.enabled",
 Exposed=(Window,DedicatedWorker)]
interface RTCEncodedAudioFrame {
    readonly attribute unsigned long timestamp;
    attribute ArrayBuffer data;
    RTCEncodedAudioFrameMetadata getMetadata();
};
