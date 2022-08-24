/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/IndexedDB/#cursor-interface
 */

enum IDBCursorDirection {
    "next",
    "nextunique",
    "prev",
    "prevunique"
};

[Exposed=(Window,Worker)]
interface IDBCursor {
    readonly    attribute (IDBObjectStore or IDBIndex) source;

    [BinaryName="getDirection"]
    readonly    attribute IDBCursorDirection           direction;

    [Pure, Throws] readonly attribute any key;
    [Pure, Throws] readonly attribute any primaryKey;
    [SameObject] readonly attribute IDBRequest request;

    [Throws]
    void       advance ([EnforceRange] unsigned long count);

    [Throws]
    void       continue (optional any key);

    [Throws]
    void       continuePrimaryKey(any key, any primaryKey);

    [NewObject, Throws] IDBRequest update(any value);
    [NewObject, Throws] IDBRequest delete();
};

[Exposed=(Window,Worker)]
interface IDBCursorWithValue : IDBCursor {
    [Pure, Throws] readonly attribute any value;
};
