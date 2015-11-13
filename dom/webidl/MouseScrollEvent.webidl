/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface MouseScrollEvent : MouseEvent
{
  const long HORIZONTAL_AXIS = 1;
  const long VERTICAL_AXIS = 2;

  readonly attribute long axis;

  void initMouseScrollEvent(DOMString type,
                            boolean canBubble,
                            boolean cancelable,
                            Window? view,
                            long detail,
                            long screenX,
                            long screenY,
                            long clientX,
                            long clientY,
                            boolean ctrlKey,
                            boolean altKey,
                            boolean shiftKey,
                            boolean metaKey,
                            unsigned short button,
                            EventTarget? relatedTarget,
                            long axis);
};
