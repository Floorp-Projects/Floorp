/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information see nsIDOMSimpleGestureEvent.idl.
 */

[Func="IsChromeOrXBL"]
interface SimpleGestureEvent : MouseEvent
{
  const unsigned long DIRECTION_UP = 1;
  const unsigned long DIRECTION_DOWN = 2;
  const unsigned long DIRECTION_LEFT = 4;
  const unsigned long DIRECTION_RIGHT = 8;

  const unsigned long ROTATION_COUNTERCLOCKWISE = 1;
  const unsigned long ROTATION_CLOCKWISE = 2;

  attribute unsigned long allowedDirections;

  readonly attribute unsigned long direction;

  readonly attribute double delta;

  readonly attribute unsigned long clickCount;

  void initSimpleGestureEvent(DOMString typeArg,
                              optional boolean canBubbleArg = false,
                              optional boolean cancelableArg = false,
                              optional Window? viewArg = null,
                              optional long detailArg = 0,
                              optional long screenXArg = 0,
                              optional long screenYArg = 0,
                              optional long clientXArg = 0,
                              optional long clientYArg = 0,
                              optional boolean ctrlKeyArg = false,
                              optional boolean altKeyArg = false,
                              optional boolean shiftKeyArg = false,
                              optional boolean metaKeyArg = false,
                              optional short buttonArg = 0,
                              optional EventTarget? relatedTargetArg = null,
                              optional unsigned long allowedDirectionsArg = 0,
                              optional unsigned long directionArg = 0,
                              optional double deltaArg = 0,
                              optional unsigned long clickCount = 0);
};
