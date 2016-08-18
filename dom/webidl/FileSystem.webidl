/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */


dictionary FileSystemFlags {
    boolean create = false;
    boolean exclusive = false;
};

callback interface FileSystemEntryCallback {
    void handleEvent(FileSystemEntry entry);
};

callback interface VoidCallback {
    void handleEvent();
};

callback interface ErrorCallback {
    // This should be FileError but we are implementing just a subset of this API.
    void handleEvent(DOMError error);
};

interface FileSystem {
    readonly    attribute DOMString name;
    readonly    attribute FileSystemDirectoryEntry root;
};
