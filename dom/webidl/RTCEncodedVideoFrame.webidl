/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://www.w3.org/TR/webrtc-encoded-transform
 */

// New enum for video frame types. Will eventually re-use the equivalent defined
// by WebCodecs.
enum RTCEncodedVideoFrameType {
    "empty",
    "key",
    "delta",
};

dictionary RTCEncodedVideoFrameMetadata {
    unsigned long long frameId;
    sequence<unsigned long long> dependencies;
    unsigned short width;
    unsigned short height;
    unsigned long spatialIndex;
    unsigned long temporalIndex;
    unsigned long synchronizationSource;
    octet payloadType;
    sequence<unsigned long> contributingSources;
    long long timestamp;    // microseconds
};

// New interfaces to define encoded video and audio frames. Will eventually
// re-use or extend the equivalent defined in WebCodecs.
[Pref="media.peerconnection.enabled",
 Pref="media.peerconnection.scripttransform.enabled",
 Exposed=(Window,DedicatedWorker)]
interface RTCEncodedVideoFrame {
    readonly attribute RTCEncodedVideoFrameType type;
    readonly attribute unsigned long timestamp;
    attribute ArrayBuffer data;
    RTCEncodedVideoFrameMetadata getMetadata();
};
