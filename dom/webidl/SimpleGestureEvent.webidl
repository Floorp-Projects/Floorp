/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information see nsIDOMSimpleGestureEvent.idl.
 */

interface WindowProxy;

interface SimpleGestureEvent : MouseEvent
{
  const unsigned long DIRECTION_UP = 1;
  const unsigned long DIRECTION_DOWN = 2;
  const unsigned long DIRECTION_LEFT = 4;
  const unsigned long DIRECTION_RIGHT = 8;

  const unsigned long ROTATION_COUNTERCLOCKWISE = 1;
  const unsigned long ROTATION_CLOCKWISE = 2;

  readonly attribute unsigned long direction;

  readonly attribute double delta;

  readonly attribute unsigned long clickCount;

  [Throws]
  void initSimpleGestureEvent(DOMString typeArg,
                              boolean canBubbleArg,
                              boolean cancelableArg,
                              WindowProxy? viewArg,
                              long detailArg,
                              long screenXArg,
                              long screenYArg,
                              long clientXArg,
                              long clientYArg,
                              boolean ctrlKeyArg,
                              boolean altKeyArg,
                              boolean shiftKeyArg,
                              boolean metaKeyArg,
                              unsigned short buttonArg,
                              EventTarget? relatedTargetArg,
                              unsigned long directionArg,
                              double deltaArg,
                              unsigned long clickCount);
};
