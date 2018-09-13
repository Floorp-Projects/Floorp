/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback interface FileSystemEntriesCallback {
    void handleEvent(sequence<FileSystemEntry> entries);
};

interface FileSystemDirectoryReader {

    // readEntries can be called just once. The second time it returns no data.

    [Throws]
    void readEntries(FileSystemEntriesCallback successCallback,
                     optional ErrorCallback errorCallback);
};
