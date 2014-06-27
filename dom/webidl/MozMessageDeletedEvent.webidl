/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsIVariant;

[Pref="dom.sms.enabled",
 Constructor(DOMString type, optional MozMessageDeletedEventInit eventInitDict)]
interface MozMessageDeletedEvent : Event
{
  // Array of deleted message ids.
  readonly attribute nsIVariant? deletedMessageIds;
  // Array of deleted thread ids.
  readonly attribute nsIVariant? deletedThreadIds;
};

dictionary MozMessageDeletedEventInit : EventInit
{
  nsIVariant? deletedMessageIds = null;
  nsIVariant? deletedThreadIds = null;
};
