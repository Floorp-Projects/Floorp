/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#texttrackcue
 */

enum AutoKeyword { "auto" };

enum AlignSetting {
  "start",
  "middle",
  "end",
  "left",
  "right"
};

enum DirectionSetting {
  "",
  "rl",
  "lr"
};

[Constructor(double startTime, double endTime, DOMString text),
 Pref="media.webvtt.enabled"]
interface VTTCue : EventTarget {
  readonly attribute TextTrack? track;

  attribute DOMString id;
  attribute double startTime;
  attribute double endTime;
  attribute boolean pauseOnExit;
  [Pref="media.webvtt.regions.enabled"]
  attribute VTTRegion? region;
  attribute DirectionSetting vertical;
  attribute boolean snapToLines;
  attribute (long or AutoKeyword) line;
  [SetterThrows]
  attribute AlignSetting lineAlign;
  [SetterThrows]
  attribute long position;
  [SetterThrows]
  attribute AlignSetting positionAlign;
  [SetterThrows]
  attribute long size;
  attribute AlignSetting align;
  attribute DOMString text;
  DocumentFragment getCueAsHTML();

  attribute EventHandler onenter;

  attribute EventHandler onexit;
};

// Mozilla extensions.
partial interface VTTCue {
  [ChromeOnly]
  attribute HTMLDivElement? displayState;
  [ChromeOnly]
  readonly attribute boolean hasBeenReset;
};
