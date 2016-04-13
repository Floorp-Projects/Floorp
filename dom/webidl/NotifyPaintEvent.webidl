/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information about this interface see nsIDOMNotifyPaintEvent.idl
 */

interface NotifyPaintEvent : Event
{
  readonly attribute DOMRectList clientRects;

  readonly attribute DOMRect boundingClientRect;

  readonly attribute PaintRequestList paintRequests;

  readonly attribute unsigned long long transactionId;
};
