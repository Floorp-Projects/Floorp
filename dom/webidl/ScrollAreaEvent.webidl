/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface ScrollAreaEvent : UIEvent
{
  readonly attribute float x;
  readonly attribute float y;
  readonly attribute float width;
  readonly attribute float height;

  void initScrollAreaEvent(DOMString type,
                           boolean canBubble,
                           boolean cancelable,
                           Window? view,
                           long detail,
                           float x,
                           float y,
                           float width,
                           float height);
};
