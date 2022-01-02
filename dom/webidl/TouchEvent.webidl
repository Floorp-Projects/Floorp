/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary TouchEventInit : EventModifierInit {
  sequence<Touch> touches = [];
  sequence<Touch> targetTouches = [];
  sequence<Touch> changedTouches = [];
};

[Func="mozilla::dom::TouchEvent::PrefEnabled",
 Exposed=Window]
interface TouchEvent : UIEvent {
  constructor(DOMString type, optional TouchEventInit eventInitDict = {});

  readonly attribute TouchList touches;
  readonly attribute TouchList targetTouches;
  readonly attribute TouchList changedTouches;

  readonly attribute boolean altKey;
  readonly attribute boolean metaKey;
  readonly attribute boolean ctrlKey;
  readonly attribute boolean shiftKey;

  void initTouchEvent(DOMString type,
                      optional boolean canBubble = false,
                      optional boolean cancelable = false,
                      optional Window? view = null,
                      optional long detail = 0,
                      optional boolean ctrlKey = false,
                      optional boolean altKey = false,
                      optional boolean shiftKey = false,
                      optional boolean metaKey = false,
                      optional TouchList? touches = null,
                      optional TouchList? targetTouches = null,
                      optional TouchList? changedTouches = null);
};
