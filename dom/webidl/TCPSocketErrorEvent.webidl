/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Dispatched as part of the "error" event in the following situations:
* - if there's an error detected when the TCPSocket closes
* - if there's an internal error while sending data
* - if there's an error connecting to the host
*/

[Func="mozilla::dom::TCPSocket::ShouldTCPSocketExist",
 Exposed=Window]
interface TCPSocketErrorEvent : Event {
  constructor(DOMString type,
              optional TCPSocketErrorEventInit eventInitDict = {});

  readonly attribute DOMString name;
  readonly attribute DOMString message;
  readonly attribute unsigned long errorCode; // The internal nsresult error code.
};

dictionary TCPSocketErrorEventInit : EventInit
{
  DOMString name = "";
  DOMString message = "";
  unsigned long errorCode = 0;
};
