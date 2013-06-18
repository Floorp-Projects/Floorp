/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional HashChangeEventInit eventInitDict), HeaderFile="GeneratedEventClasses.h"]
interface HashChangeEvent : Event
{
  readonly attribute DOMString? oldURL;
  readonly attribute DOMString? newURL;

  // initHashChangeEvent is a Gecko specific deprecated method.
  [Throws]
  void initHashChangeEvent(DOMString type,
                           boolean canBubble,
                           boolean cancelable,
                           DOMString? oldURL,
                           DOMString? newURL);
};

dictionary HashChangeEventInit : EventInit
{
  DOMString oldURL = "";
  DOMString newURL = "";
};
