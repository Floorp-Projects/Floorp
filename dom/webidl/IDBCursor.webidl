/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBCursorDirection
 */

enum IDBCursorDirection {
    "next",
    "nextunique",
    "prev",
    "prevunique"
};

interface IDBCursor {
    // This should be: readonly    attribute (IDBObjectStore or IDBIndex) source;
    readonly    attribute nsISupports source;

    readonly    attribute IDBCursorDirection           direction;

    [Throws]
    readonly    attribute any                          key;

    [Throws]
    readonly    attribute any                          primaryKey;

    [Throws]
    IDBRequest update (any value);

    [Throws]
    void       advance ([EnforceRange] unsigned long count);

    [Throws]
    void       continue (optional any key);

    [Throws]
    IDBRequest delete ();
};

interface IDBCursorWithValue : IDBCursor {
    [Throws]
    readonly    attribute any value;
};
