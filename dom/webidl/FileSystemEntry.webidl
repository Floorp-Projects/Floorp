/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface FileSystemEntry {
    readonly attribute boolean isFile;
    readonly attribute boolean isDirectory;

    [GetterThrows]
    readonly attribute DOMString name;

    [GetterThrows]
    readonly attribute DOMString fullPath;

    readonly attribute FileSystem filesystem;

/** Not implemented:
 *  void getMetadata(MetadataCallback successCallback,
 *                   optional ErrorCallback errorCallback);
 *  void moveTo(FileSystemDirectoryEntry parent, optional DOMString? name,
 *              optional FileSystemEntryCallback successCallback,
 *              optional ErrorCallback errorCallback);
 *  void copyTo(FileSystemDirectoryEntry parent, optional DOMString? name,
 *              optional FileSystemEntryCallback successCallback,
 *              optional ErrorCallback errorCallback);
 *  DOMString toURL();
 *  void remove(VoidCallback successCallback,
 *              optional ErrorCallback errorCallback);
 *  void getParent(optional FileSystemEntryCallback successCallback,
 *                 optional ErrorCallback errorCallback);
 */
};
