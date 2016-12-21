/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://webaudio.github.io/web-audio-api/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Constructor,
 Constructor(AudioChannel audioChannelType),
 Pref="dom.webaudio.enabled"]
interface AudioContext : BaseAudioContext {

    // Bug 1324545: readonly        attribute double outputLatency;
    // Bug 1324545: AudioTimestamp                  getOutputTimestamp ();

    [Throws]
    Promise<void> suspend();
    [Throws]
    Promise<void> close();

    [NewObject, Throws, UnsafeInPrerendering]
    MediaElementAudioSourceNode createMediaElementSource(HTMLMediaElement mediaElement);

    [NewObject, Throws, UnsafeInPrerendering]
    MediaStreamAudioSourceNode createMediaStreamSource(MediaStream mediaStream);

    // Bug 1324548: MediaStreamTrackAudioSourceNode createMediaStreamTrackSource (AudioMediaStreamTrack mediaStreamTrack);

    [NewObject, Throws]
    MediaStreamAudioDestinationNode createMediaStreamDestination();
};

// Mozilla extensions
partial interface AudioContext {
  // Read AudioChannel.webidl for more information about this attribute.
  [Pref="media.useAudioChannelAPI"]
  readonly attribute AudioChannel mozAudioChannelType;

  // These 2 events are dispatched when the AudioContext object is muted by
  // the AudioChannelService. It's call 'interrupt' because when this event is
  // dispatched on a HTMLMediaElement, the audio stream is paused.
  [Pref="media.useAudioChannelAPI"]
  attribute EventHandler onmozinterruptbegin;

  [Pref="media.useAudioChannelAPI"]
  attribute EventHandler onmozinterruptend;

  // This method is for test only.
  [ChromeOnly] AudioChannel testAudioChannelInAudioNodeStream();
};
