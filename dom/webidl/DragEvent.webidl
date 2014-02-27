/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface WindowProxy;

interface DragEvent : MouseEvent
{
  readonly attribute DataTransfer? dataTransfer;

  [Throws]
  void initDragEvent(DOMString type,
                     boolean canBubble,
                     boolean cancelable,
                     WindowProxy? aView,
                     long aDetail,
                     long aScreenX,
                     long aScreenY,
                     long aClientX,
                     long aClientY,
                     boolean aCtrlKey,
                     boolean aAltKey,
                     boolean aShiftKey,
                     boolean aMetaKey,
                     unsigned short aButton,
                     EventTarget? aRelatedTarget,
                     DataTransfer? aDataTransfer);
};
