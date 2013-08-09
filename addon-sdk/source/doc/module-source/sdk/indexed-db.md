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

Most of the objects that implement the IndexedDB API, such as
[IDBTransaction](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBTransaction),
[IDBOpenDBRequest](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBOpenDBRequest),
and [IDBObjectStore](https://developer.mozilla.org/en-US/docs/IndexedDB/IDBObjectStore),
are accessible through the indexedDB object itself.

The API exposed by `indexed-db` is almost identical to the DOM IndexedDB API,
so we haven't repeated its documentation here, but refer you to the
[IndexedDB API documentation](https://developer.mozilla.org/en-US/docs/IndexedDB)
for all the details.

The database created will be unique and private per add-on, and is not linked
to any website database. The module cannot be used to interact with a given
website database. See
[bug 778197](https://bugzilla.mozilla.org/show_bug.cgi?id=779197) and
[bug 786688](https://bugzilla.mozilla.org/show_bug.cgi?id=786688).

## Example

Here's a complete add-on that adds two widgets to the browser: the widget labeled
"Add" add the title of the current tab to a database, while the widget labeled
"List" lists all the titles in the database.

The add-on implements helper functions `open()`, `addItem()` and `getItems()`
to open the database, add a new item to the database, and get all items in the
database.

    var { indexedDB, IDBKeyRange } = require('sdk/indexed-db');
    var widgets = require("sdk/widget");

    var database = {};

    database.onerror = function(e) {
      console.error(e.value)
    }

    function open(version) {
      var request = indexedDB.open("stuff", version);

      request.onupgradeneeded = function(e) {
        var db = e.target.result;
        e.target.transaction.onerror = database.onerror;

        if(db.objectStoreNames.contains("items")) {
          db.deleteObjectStore("items");
        }

        var store = db.createObjectStore("items",
          {keyPath: "time"});
      };

      request.onsuccess = function(e) {
        database.db = e.target.result;
      };

      request.onerror = database.onerror;
    };

    function addItem(name) {
      var db = database.db;
      var trans = db.transaction(["items"], "readwrite");
      var store = trans.objectStore("items");
      var time = new Date().getTime();
      var request = store.put({
        "name": name,
        "time": time
      });

      request.onerror = database.onerror;
    };

    function getItems(callback) {
      var cb = callback;
      var db = database.db;
      var trans = db.transaction(["items"], "readwrite");
      var store = trans.objectStore("items");
      var items = new Array();

      trans.oncomplete = function() {
        cb(items);
      }

      var keyRange = IDBKeyRange.lowerBound(0);
      var cursorRequest = store.openCursor(keyRange);

      cursorRequest.onsuccess = function(e) {
        var result = e.target.result;
        if(!!result == false)
          return;

        items.push(result.value.name);
        result.continue();
      };

      cursorRequest.onerror = database.onerror;
    };

    function listItems(itemList) {
      console.log(itemList);
    }

    open("1");

    widgets.Widget({
      id: "add-it",
      width: 50,
      label: "Add",
      content: "Add",
      onClick: function() {
        addItem(require("sdk/tabs").activeTab.title);
      }
    });

    widgets.Widget({
      id: "list-them",
      width: 50,
      label: "List",
      content: "List",
      onClick: function() {
        getItems(listItems);
      }
    });

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

<api name="DOMException">
@property {object}

Provides more detailed information about an exception.
See the [DOMException documentation](https://developer.mozilla.org/en-US/docs/DOM/DOMException).
</api>
