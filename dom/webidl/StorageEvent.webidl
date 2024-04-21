/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Interface for a client side storage. See
 * http://dev.w3.org/html5/webstorage/#the-storage-event
 * for more information.
 *
 * Event sent to a window when a storage area changes.
 */

[Exposed=Window]
interface StorageEvent : Event
{
  constructor(DOMString type, optional StorageEventInit eventInitDict = {});

  readonly attribute DOMString? key;
  readonly attribute DOMString? oldValue;
  readonly attribute DOMString? newValue;
  readonly attribute USVString url;
  readonly attribute Storage? storageArea;

  undefined initStorageEvent(DOMString type,
                             optional boolean canBubble = false,
                             optional boolean cancelable = false,
                             optional DOMString? key = null,
                             optional DOMString? oldValue = null,
                             optional DOMString? newValue = null,
                             optional USVString url = "",
                             optional Storage? storageArea = null);
};

dictionary StorageEventInit : EventInit
{
  DOMString? key = null;
  DOMString? oldValue = null;
  DOMString? newValue = null;
  USVString url = "";
  Storage? storageArea = null;
};
