/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/IndexedDB/#object-store-interface
 */

dictionary IDBObjectStoreParameters {
    (DOMString or sequence<DOMString>)? keyPath = null;
    boolean                             autoIncrement = false;
};

[Exposed=(Window,Worker)]
interface IDBObjectStore {
    [SetterThrows]
    attribute DOMString name;

    [Throws]
    readonly    attribute any            keyPath;

    readonly    attribute DOMStringList  indexNames;
    [SameObject] readonly attribute IDBTransaction transaction;
    readonly    attribute boolean        autoIncrement;

    [NewObject, Throws]
    IDBRequest put (any value, optional any key);
    [NewObject, Throws]
    IDBRequest add (any value, optional any key);
    [NewObject, Throws]
    IDBRequest delete (any key);
    [NewObject, Throws]
    IDBRequest clear ();
    [NewObject, Throws]
    IDBRequest get (any key);
    [NewObject, Throws]
    IDBRequest getKey (any key);

    // Success fires IDBTransactionEvent, result == array of values for given keys
    // If we decide to add use a counter for the mozGetAll function, we'll need
    // to pull it out into a sepatate operation with a BinaryName mapping to the
    // same underlying implementation.
    [NewObject, Throws, Alias="mozGetAll"]
    IDBRequest getAll(optional any query,
                      optional [EnforceRange] unsigned long count);
    [NewObject, Throws]
    IDBRequest getAllKeys(optional any query,
                          optional [EnforceRange] unsigned long count);

    [NewObject, Throws]
    IDBRequest count(optional any key);

    [NewObject, Throws]
    IDBRequest openCursor (optional any range, optional IDBCursorDirection direction = "next");
    [NewObject, Throws]
    IDBRequest openKeyCursor(optional any query,
                             optional IDBCursorDirection direction = "next");
    [NewObject, Throws]
    IDBIndex   createIndex (DOMString name, (DOMString or sequence<DOMString>) keyPath, optional IDBIndexParameters optionalParameters = {});

    [Throws]
    IDBIndex   index (DOMString name);

    [Throws]
    void       deleteIndex (DOMString indexName);
};

