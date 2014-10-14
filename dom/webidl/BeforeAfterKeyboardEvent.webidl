/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString typeArg,
 optional BeforeAfterKeyboardEventInit eventInitDict),
 CheckPermissions="embed-apps before-after-keyboard-event",
 Pref="dom.beforeAfterKeyboardEvent.enabled"]
interface BeforeAfterKeyboardEvent : KeyboardEvent
{
  // The valid value of embeddedCancelled is:
  // - "mozbrowserbeforekeydown": null
  // - "mozbrowserbeforekeyup": null
  // - "mozbrowserafterkeydown": true/false
  // - "mozbrowserafterkeyup": true/false
  readonly attribute boolean? embeddedCancelled;
};

dictionary BeforeAfterKeyboardEventInit : KeyboardEventInit
{
  boolean? embeddedCancelled = null;
};
