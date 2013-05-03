/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface NotifyAudioAvailableEvent : Event
{
  [Throws]
  readonly attribute Float32Array frameBuffer;

  readonly attribute float  time;

  [Throws]
  void initAudioAvailableEvent(DOMString type,
                               boolean canBubble,
                               boolean cancelable,
                               sequence<float>? frameBuffer,
                               unsigned long frameBufferLength,
                               float time,
                               boolean allowAudioData);
};
