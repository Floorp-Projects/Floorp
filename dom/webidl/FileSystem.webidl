/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
    void handleEvent(DOMException err);
};

interface FileSystem {
    readonly    attribute USVString name;
    readonly    attribute FileSystemDirectoryEntry root;
};
