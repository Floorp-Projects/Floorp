/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/IndexedDB/#index-interface
 */

dictionary IDBIndexParameters {
    boolean unique = false;
    boolean multiEntry = false;
    // <null>:   Not locale-aware, uses normal JS sorting.
    // <string>: Always sorted based on the rules of the specified
    //           locale (e.g. "en-US", etc.).
    // "auto":   Sorted by the platform default, may change based on
    //           user agent options.
    DOMString? locale = null;
};

[Exposed=(Window,Worker)]
interface IDBIndex {
    [SetterThrows] attribute DOMString name;
    [SameObject] readonly attribute IDBObjectStore objectStore;

    [Throws]
    readonly    attribute any            keyPath;

    readonly    attribute boolean        multiEntry;
    readonly    attribute boolean        unique;

    // <null>:   Not locale-aware, uses normal JS sorting.
    // <string>: Sorted based on the rules of the specified locale.
    //           Note: never returns "auto", only the current locale.
    [Pref="dom.indexedDB.experimental"]
    readonly attribute DOMString? locale;

    [Pref="dom.indexedDB.experimental"]
    readonly attribute boolean isAutoLocale;

    [NewObject, Throws] IDBRequest get(any query);
    [NewObject, Throws] IDBRequest getKey(any query);

    // If we decide to add use counters for the mozGetAll/mozGetAllKeys
    // functions, we'll need to pull them out into sepatate operations
    // with a BinaryName mapping to the same underlying implementation.
    // See also bug 1577227.
    [NewObject, Throws, Alias="mozGetAll"]
    IDBRequest getAll(optional any query,
                      optional [EnforceRange] unsigned long count);
    [NewObject, Throws, Alias="mozGetAllKeys"]
    IDBRequest getAllKeys(optional any query,
                            optional [EnforceRange] unsigned long count);

    [NewObject, Throws] IDBRequest count(optional any query);

    [NewObject, Throws]
    IDBRequest openCursor(optional any query,
                          optional IDBCursorDirection direction = "next");
    [NewObject, Throws]
    IDBRequest openKeyCursor(optional any query,
                             optional IDBCursorDirection direction = "next");
};
