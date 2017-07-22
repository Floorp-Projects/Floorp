/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Func="IsChromeOrXBL"]
interface XULCommandEvent : UIEvent
{
  readonly attribute boolean ctrlKey;
  readonly attribute boolean shiftKey;
  readonly attribute boolean altKey;
  readonly attribute boolean metaKey;

  readonly attribute unsigned short inputSource;

  readonly attribute Event? sourceEvent;

  void initCommandEvent(DOMString type,
                        optional boolean canBubble = false,
                        optional boolean cancelable = false,
                        optional Window? view = null,
                        optional long detail = 0,
                        optional boolean ctrlKey = false,
                        optional boolean altKey = false,
                        optional boolean shiftKey = false,
                        optional boolean metaKey = false,
                        optional Event? sourceEvent = null,
                        optional unsigned short inputSource = 0);
};
