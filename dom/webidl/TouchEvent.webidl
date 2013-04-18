/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface TouchList;
interface WindowProxy;

[PrefControlled]
interface TouchEvent : UIEvent {
  readonly attribute TouchList? touches;
  readonly attribute TouchList? targetTouches;
  readonly attribute TouchList? changedTouches;

  readonly attribute boolean altKey;
  readonly attribute boolean metaKey;
  readonly attribute boolean ctrlKey;
  readonly attribute boolean shiftKey;

  [Throws]
  void initTouchEvent(DOMString type,
                      boolean canBubble,
                      boolean cancelable,
                      WindowProxy? view,
                      long detail,
                      boolean ctrlKey,
                      boolean altKey,
                      boolean shiftKey,
                      boolean metaKey,
                      TouchList? touches,
                      TouchList? targetTouches,
                      TouchList? changedTouches);
};
