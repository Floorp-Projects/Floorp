/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/IndexedDB/#idbtransaction
 * https://w3c.github.io/IndexedDB/#enumdef-idbtransactionmode
 */

enum IDBTransactionMode {
    "readonly",
    "readwrite",
    // The "readwriteflush" mode is only available when the
    // |dom.indexedDB.experimental| pref returns
    // true. This mode is not yet part of the standard.
    "readwriteflush",
    "cleanup",
    "versionchange"
};

[Exposed=(Window,Worker)]
interface IDBTransaction : EventTarget {
    [Throws]
    readonly    attribute IDBTransactionMode mode;
    [SameObject] readonly attribute IDBDatabase db;

    readonly    attribute DOMException?      error;

    [Throws]
    IDBObjectStore objectStore (DOMString name);

    [Throws]
    void           commit();

    [Throws]
    void           abort();

                attribute EventHandler       onabort;
                attribute EventHandler       oncomplete;
                attribute EventHandler       onerror;
};

// This seems to be custom
partial interface IDBTransaction {
    readonly    attribute DOMStringList objectStoreNames;
};
