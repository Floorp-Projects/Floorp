/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBObjectStoreParameters
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface IDBDatabase : EventTarget {
    readonly    attribute DOMString          name;
    readonly    attribute unsigned long long version;

    [Throws]
    readonly    attribute DOMStringList      objectStoreNames;

    [Throws]
    IDBObjectStore createObjectStore (DOMString name, optional IDBObjectStoreParameters optionalParameters);

    [Throws]
    void           deleteObjectStore (DOMString name);

    // This should be:
    // IDBTransaction transaction ((DOMString or sequence<DOMString>) storeNames, optional IDBTransactionMode mode = "readonly");
    // but unions are not currently supported.

    [Throws]
    IDBTransaction transaction (DOMString storeName, optional IDBTransactionMode mode = "readonly");

    [Throws]
    IDBTransaction transaction (sequence<DOMString> storeNames, optional IDBTransactionMode mode = "readonly");

    void           close ();

    [SetterThrows]
                attribute EventHandler       onabort;
    [SetterThrows]
                attribute EventHandler       onerror;
    [SetterThrows]
                attribute EventHandler       onversionchange;
};

partial interface IDBDatabase {
    [Throws]
    IDBRequest mozCreateFileHandle (DOMString name, optional DOMString type);
};
