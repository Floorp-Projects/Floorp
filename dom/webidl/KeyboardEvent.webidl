/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface WindowProxy;

interface KeyboardEvent : UIEvent
{
  readonly attribute unsigned long    charCode;
  readonly attribute unsigned long    keyCode;

  readonly attribute boolean          altKey;
  readonly attribute boolean          ctrlKey;
  readonly attribute boolean          shiftKey;
  readonly attribute boolean          metaKey;

  boolean getModifierState(DOMString key);

  const unsigned long DOM_KEY_LOCATION_STANDARD = 0x00;
  const unsigned long DOM_KEY_LOCATION_LEFT     = 0x01;
  const unsigned long DOM_KEY_LOCATION_RIGHT    = 0x02;
  const unsigned long DOM_KEY_LOCATION_NUMPAD   = 0x03;
  const unsigned long DOM_KEY_LOCATION_MOBILE   = 0x04;
  const unsigned long DOM_KEY_LOCATION_JOYSTICK = 0x05;

  readonly attribute unsigned long location;
  readonly attribute boolean       repeat;

  readonly attribute DOMString key;
};

// Mozilla extensions
KeyboardEvent implements KeyEvent;
