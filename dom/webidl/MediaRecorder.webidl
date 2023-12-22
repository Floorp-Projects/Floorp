/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/dap/raw-file/default/media-stream-capture/MediaRecorder.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */


enum BitrateMode { "constant", "variable" };

enum RecordingState { "inactive", "recording", "paused" };

[Exposed=Window]
interface MediaRecorder : EventTarget {
  [Throws]
  constructor(MediaStream stream, optional MediaRecorderOptions options = {});
  [Throws]
  constructor(AudioNode node, optional unsigned long output = 0,
              optional MediaRecorderOptions options = {});
  readonly attribute MediaStream stream;
  readonly attribute DOMString mimeType;
  readonly attribute RecordingState state;
  attribute EventHandler onstart;
  attribute EventHandler onstop;
  attribute EventHandler ondataavailable;
  attribute EventHandler onpause;
  attribute EventHandler onresume;
  attribute EventHandler onerror;
  readonly attribute unsigned long videoBitsPerSecond;
  readonly attribute unsigned long audioBitsPerSecond;


  [Throws]
  undefined start(optional unsigned long timeslice);
  [Throws]
  undefined stop();
  [Throws]
  undefined pause();
  [Throws]
  undefined resume();
  [Throws]
  undefined requestData();

  static boolean isTypeSupported(DOMString type);
};

dictionary MediaRecorderOptions {
  DOMString mimeType = "";
  unsigned long audioBitsPerSecond;
  unsigned long videoBitsPerSecond;
  unsigned long bitsPerSecond;
};
