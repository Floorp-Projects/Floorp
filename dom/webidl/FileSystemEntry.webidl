/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface FileSystemEntry {
    readonly attribute boolean isFile;
    readonly attribute boolean isDirectory;

    [GetterThrows]
    readonly attribute USVString name;

    [GetterThrows]
    readonly attribute USVString fullPath;

    readonly attribute FileSystem filesystem;

    void getParent(optional FileSystemEntryCallback successCallback,
                   optional ErrorCallback errorCallback);
};
