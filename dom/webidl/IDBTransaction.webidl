/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBTransaction
 * https://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBTransactionMode
 */

enum IDBTransactionMode {
    "readonly",
    "readwrite",
    "versionchange"
};

interface IDBTransaction : EventTarget {
    [Throws]
    readonly    attribute IDBTransactionMode mode;
    readonly    attribute IDBDatabase        db;

    [Throws]
    readonly    attribute DOMError?          error;

    [Throws]
    IDBObjectStore objectStore (DOMString name);

    [Throws]
    void           abort();

    [SetterThrows]
                attribute EventHandler       onabort;
    [SetterThrows]
                attribute EventHandler       oncomplete;
    [SetterThrows]
                attribute EventHandler       onerror;
};

// This seems to be custom
partial interface IDBTransaction {
    [Throws]
    readonly    attribute DOMStringList objectStoreNames;
};
