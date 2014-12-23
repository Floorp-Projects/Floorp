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

  /**
   * Append data to a given file.
   * If the file doesn't exist, a "NotFoundError" event will be dispatched.
   * In the same time, it is a request.onerror case.
   * If the file exists, it will be opened with the following permission:
   *                                                "PR_WRONLY|PR_CREATE_FILE|PR_APPEND".
   * The function will return null when blob file is null and other unexpected situations.
   * @parameter aBlob: A Blob object representing the data to append
   * @parameter aName: A string representing the full name (path + file name) of the file
   *                   to append data to.
   */
  [Throws]
  DOMRequest? appendNamed(Blob? aBlob, DOMString aName);

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

  // Indicates if the storage area denoted by storageName is capable of
  // being mounted and unmounted.
  readonly attribute boolean canBeMounted;

  // Indicates if the storage area denoted by storageName is capable of
  // being shared and unshared.
  readonly attribute boolean canBeShared;

  // Indicates if the storage area denoted by storageName is capable of
  // being formatted.
  readonly attribute boolean canBeFormatted;

  // Determines if this storage area is the one which will be used by default
  // for storing new files.
  readonly attribute boolean default;

  [NewObject, Throws]
  // XXXbz what type does this really return?
  Promise<any> getRoot();
};
