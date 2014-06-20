/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum CallIdPresentation {
  "allowed",
  // Call number/name has been withheld by the calling party.
  "restricted",
  // Call number is not available due to calling party being of type payphone.
  "payphone",
  // Call number/name is not available due to networking problems or other reason.
  "unknown"
};

[Pref="dom.telephony.enabled"]
interface TelephonyCallId {
  // It is an empty string when "numberPresentation" is not "allowed."
  readonly attribute DOMString number;

  // This attribute is not relevant for outgoing calls. Default value is
  // "allowed."
  readonly attribute CallIdPresentation numberPresentation;

  // This attribute is not relevant for outgoing calls. It is an empty string
  // 1) when the call is outgoing, or 2) when the call is incoming and
  // "namePresentation" is not "allowed." However, it could still be empty
  // even the call is incoming and "namePresentation" is "allowed."
  readonly attribute DOMString name;

  // This attribute is not relevant for outgoing calls. Default value is
  // "allowed."
  readonly attribute CallIdPresentation namePresentation;
};
