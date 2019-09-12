/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/web-animations/#animation
 *
 * Copyright © 2015 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum AnimationPlayState { "idle", "running", "paused", "finished" };

enum AnimationReplaceState { "active", "removed", "persisted" };

interface Animation : EventTarget {
  [Throws]
  constructor(optional AnimationEffect? effect = null,
              optional AnimationTimeline? timeline);

  attribute DOMString id;
  [Func="Document::IsWebAnimationsEnabled", Pure]
  attribute AnimationEffect? effect;
  [Func="Document::AreWebAnimationsTimelinesEnabled"]
  attribute AnimationTimeline? timeline;
  [BinaryName="startTimeAsDouble"]
  attribute double? startTime;
  [SetterThrows, BinaryName="currentTimeAsDouble"]
  attribute double? currentTime;

           attribute double             playbackRate;
  [BinaryName="playStateFromJS"]
  readonly attribute AnimationPlayState playState;
  [BinaryName="pendingFromJS"]
  readonly attribute boolean            pending;
  [Pref="dom.animations-api.autoremove.enabled"]
  readonly attribute AnimationReplaceState replaceState;
  [Func="Document::IsWebAnimationsEnabled", Throws]
  readonly attribute Promise<Animation> ready;
  [Func="Document::IsWebAnimationsEnabled", Throws]
  readonly attribute Promise<Animation> finished;
           attribute EventHandler       onfinish;
           attribute EventHandler       oncancel;
  [Pref="dom.animations-api.autoremove.enabled"]
           attribute EventHandler       onremove;
  void cancel();
  [Throws]
  void finish();
  [Throws, BinaryName="playFromJS"]
  void play();
  [Throws, BinaryName="pauseFromJS"]
  void pause();
  void updatePlaybackRate (double playbackRate);
  [Throws]
  void reverse();
  [Pref="dom.animations-api.autoremove.enabled"]
  void persist();
  [Pref="dom.animations-api.autoremove.enabled", Throws]
  void commitStyles();
};

// Non-standard extensions
partial interface Animation {
  [ChromeOnly] readonly attribute boolean isRunningOnCompositor;
};
