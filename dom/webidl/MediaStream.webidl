/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origins of this IDL file are
 * http://dev.w3.org/2011/webrtc/editor/getusermedia.html
 *
 * Copyright � 2012 W3C� (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

// These dictionaries need to be in a separate file from their
// MediaTrackConstraints* counterparts due to a webidl compiler limitation.

dictionary MediaStreamConstraints {
    (boolean or MediaTrackConstraints) audio = false;
    (boolean or MediaTrackConstraints) video = false;
    boolean picture = false; // Mozilla legacy
    boolean fake = false;    // for testing
    DOMString? peerIdentity = null;
};

interface MediaStream {
    // readonly attribute DOMString    id;
    sequence<AudioStreamTrack> getAudioTracks();
    sequence<VideoStreamTrack> getVideoTracks();
    sequence<MediaStreamTrack> getTracks();
    // MediaStreamTrack           getTrackById (DOMString trackId);
    // void                       addTrack (MediaStreamTrack track);
    // void                       removeTrack (MediaStreamTrack track);
    //         attribute boolean      ended;
    //         attribute EventHandler onended;
    //         attribute EventHandler onaddtrack;
    //         attribute EventHandler onremovetrack;
	readonly attribute double currentTime;
};
