/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/IndexedDB/#database-interface
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=(Window,Worker)]
interface IDBDatabase : EventTarget {
    [Constant] readonly attribute DOMString name;
    readonly    attribute unsigned long long version;

    readonly    attribute DOMStringList      objectStoreNames;

    [NewObject, Throws]
    IDBTransaction transaction((DOMString or sequence<DOMString>) storeNames,
                               optional IDBTransactionMode mode = "readonly");
    [NewObject, Throws]
    IDBObjectStore createObjectStore(
        DOMString name,
        optional IDBObjectStoreParameters options = {});
    [Throws]
    void           deleteObjectStore (DOMString name);
    void           close ();

                attribute EventHandler       onabort;
                attribute EventHandler       onclose;
                attribute EventHandler       onerror;
                attribute EventHandler       onversionchange;
};

partial interface IDBDatabase {
    [Exposed=Window, Throws,
     Deprecated="IDBDatabaseCreateMutableFile",
     Pref="dom.fileHandle.enabled"]
    IDBRequest createMutableFile (DOMString name, optional DOMString type);
};
