<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `indexed-db` module exposes the
[IndexedDB API](https://developer.mozilla.org/en-US/docs/IndexedDB)
to add-ons.

Scripts running in web pages can access IndexedDB via the `window` object.
For example:

    window.indexedDB = window.indexedDB || window.webkitIndexedDB || window.mozIndexedDB || window.msIndexedDB;

    var request = window.indexedDB.open("MyDatabase");
    request.onerror = function(event) {
      console.log("failure");
    };
    request.onsuccess = function(event) {
      console.log("success");
    };

Because your main add-on code can't access the DOM, you can't do this.
So you can use the `indexed-db` module to access the same API:

    var { indexedDB } = require('indexed-db');

    var request = indexedDB.open('MyDatabase');
    request.onerror = function(event) {
      console.log("failure");
    };
    request.onsuccess = function(event) {
      console.log("success");
    };

This module also exports all the other objects that implement
the IndexedDB API, listed below under
[API Reference](modules/sdk/indexed-db.html#API Reference).

The API exposed by `indexed-db` is almost identical to the DOM IndexedDB API,
so we haven't repeated its documentation here, but refer you to the
[IndexedDB API documentation](https://developer.mozilla.org/en-US/docs/IndexedDB)
for all the details.

The database created will be unique and private per addon, and is not linked to any website database. The module cannot be used to interact with a given website database. See [bug 778197](https://bugzilla.mozilla.org/show_bug.cgi?id=779197) and [bug 786688](https://bugzilla.mozilla.org/show_bug.cgi?id=786688).

## Example of Usage

[Promise-based example using indexedDB for record storage](https://github.com/gregglind/micropilot/blob/ec65446d611a65b0646be1806359c463193d5a91/lib/micropilot.js#L80-L198).

<api name="indexedDB">
@property {object}

Enables you to create, open, and delete databases.
See the [IDBFactory documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBFactory).
</api>

<api name="IDBKeyRange">
@property {object}

Defines a range of keys.
See the [IDBKeyRange documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBKeyRange).
</api>

<api name="IDBCursor">
@property {object}

For traversing or iterating records in a database.
See the [IDBCursor documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBCursor).

</api>

<api name="IDBTransaction">
@property {object}

Represents a database transaction.
See the [IDBTransaction documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBTransaction).
</api>

<api name="IDBOpenDBRequest">
@property {object}

Represents an asynchronous request to open a database.
See the [IDBOpenDBRequest documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBOpenDBRequest).
</api>

<api name="IDBVersionChangeEvent">
@property {object}

Event indicating that the database version has changed.
See the [IDBVersionChangeEvent documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBVersionChangeEvent).
</api>

<api name="IDBDatabase">
@property {object}

Represents a connection to a database.
See the [IDBDatabase documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBDatabase).
</api>

<api name="IDBFactory">
@property {object}

Enables you to create, open, and delete databases.
See the [IDBFactory documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBFactory).
</api>

<api name="IDBIndex">
@property {object}

Provides access to a database index.
See the [IDBIndex documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBIndex).
</api>

<api name="IDBObjectStore">
@property {object}

Represents an object store in a database.
See the [IDBObjectStore documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBObjectStore).
</api>

<api name="IDBRequest">
@property {object}

Provides access to the results of asynchronous requests to databases
and database objects.
See the [IDBRequest documentation](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBRequest).
</api>

<api name="DOMException">
@property {object}

Provides more detailed information about an exception.
See the [DOMException documentation](https://developer.mozilla.org/en-US/docs/DOM/DOMException).
</api>
