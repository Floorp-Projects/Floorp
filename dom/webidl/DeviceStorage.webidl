/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary DeviceStorageEnumerationParameters {
  Date since;
};

interface DeviceStorage : EventTarget {
  attribute EventHandler onchange;

  [Throws]
  DOMRequest? add(Blob? aBlob);
  [Throws]
  DOMRequest? addNamed(Blob? aBlob, DOMString aName);

  [Throws]
  DOMRequest get(DOMString aName);
  [Throws]
  DOMRequest getEditable(DOMString aName);
  [Throws]
  DOMRequest delete(DOMString aName);

  [Throws]
  DOMCursor enumerate(optional DeviceStorageEnumerationParameters options);
  [Throws]
  DOMCursor enumerate(DOMString path,
                      optional DeviceStorageEnumerationParameters options);
  [Throws]
  DOMCursor enumerateEditable(optional DeviceStorageEnumerationParameters options);
  [Throws]
  DOMCursor enumerateEditable(DOMString path,
                              optional DeviceStorageEnumerationParameters options);

  [Throws]
  DOMRequest freeSpace();
  [Throws]
  DOMRequest usedSpace();
  [Throws]
  DOMRequest available();
  [Throws]
  DOMRequest storageStatus();
  [Throws]
  DOMRequest format();
  [Throws]
  DOMRequest mount();
  [Throws]
  DOMRequest unmount();

  // Note that the storageName is just a name (like sdcard), and doesn't
  // include any path information.
  readonly attribute DOMString storageName;

  // Determines if this storage area is the one which will be used by default
  // for storing new files.
  readonly attribute boolean default;
};
