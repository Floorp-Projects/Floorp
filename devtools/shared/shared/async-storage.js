/**
 *
 * Adapted from https://github.com/mozilla-b2g/gaia/blob/f09993563fb5fec4393eb71816ce76cb00463190/shared/js/async_storage.js
 * (converted to use Promises instead of callbacks).
 *
 * This file defines an asynchronous version of the localStorage API, backed by
 * an IndexedDB database.  It creates a global asyncStorage object that has
 * methods like the localStorage object.
 *
 * To store a value use setItem:
 *
 *   asyncStorage.setItem("key", "value");
 *
 * This returns a promise in case you want confirmation that the value has been stored.
 *
 *  asyncStorage.setItem("key", "newvalue").then(function() {
 *    console.log("new value stored");
 *  });
 *
 * To read a value, call getItem(), but note that you must wait for a promise
 * resolution for the value to be retrieved.
 *
 *  asyncStorage.getItem("key").then(function(value) {
 *    console.log("The value of key is:", value);
 *  });
 *
 * Note that unlike localStorage, asyncStorage does not allow you to store and
 * retrieve values by setting and querying properties directly. You cannot just
 * write asyncStorage.key; you have to explicitly call setItem() or getItem().
 *
 * removeItem(), clear(), length(), and key() are like the same-named methods of
 * localStorage, and all return a promise.
 *
 * The asynchronous nature of getItem() makes it tricky to retrieve multiple
 * values. But unlike localStorage, asyncStorage does not require the values you
 * store to be strings.  So if you need to save multiple values and want to
 * retrieve them together, in a single asynchronous operation, just group the
 * values into a single object. The properties of this object may not include
 * DOM elements, but they may include things like Blobs and typed arrays.
 *
 */
const {Cc, Ci, Cu, Cr} = require("chrome");
const {indexedDB} = require("sdk/indexed-db");
const Promise = require("promise");

module.exports = (function() {
  "use strict";

  var DBNAME = "devtools-async-storage";
  var DBVERSION = 1;
  var STORENAME = "keyvaluepairs";
  var db = null;

  function withStore(type, onsuccess, onerror) {
    if (db) {
      var transaction = db.transaction(STORENAME, type);
      var store = transaction.objectStore(STORENAME);
      onsuccess(store);
    } else {
      var openreq = indexedDB.open(DBNAME, DBVERSION);
      openreq.onerror = function withStoreOnError() {
        onerror();
      };
      openreq.onupgradeneeded = function withStoreOnUpgradeNeeded() {
        // First time setup: create an empty object store
        openreq.result.createObjectStore(STORENAME);
      };
      openreq.onsuccess = function withStoreOnSuccess() {
        db = openreq.result;
        var transaction = db.transaction(STORENAME, type);
        var store = transaction.objectStore(STORENAME);
        onsuccess(store);
      };
    }
  }

  function getItem(key) {
    return new Promise((resolve, reject) => {
      var req;
      withStore("readonly", (store) => {
        store.transaction.oncomplete = function onComplete() {
          var value = req.result;
          if (value === undefined) {
            value = null;
          }
          resolve(value);
        };
        req = store.get(key);
        req.onerror = function getItemOnError() {
          reject("Error in asyncStorage.getItem(): ", req.error.name);
        };
      }, reject);
    });
  }

  function setItem(key, value) {
    return new Promise((resolve, reject) => {
      withStore("readwrite", (store) => {
        store.transaction.oncomplete = resolve;
        var req = store.put(value, key);
        req.onerror = function setItemOnError() {
          reject("Error in asyncStorage.setItem(): ", req.error.name);
        };
      }, reject);
    });
  }

  function removeItem(key) {
    return new Promise((resolve, reject) => {
      withStore("readwrite", (store) => {
        store.transaction.oncomplete = resolve;
        var req = store.delete(key);
        req.onerror = function removeItemOnError() {
          reject("Error in asyncStorage.removeItem(): ", req.error.name);
        };
      }, reject);
    });
  }

  function clear() {
    return new Promise((resolve, reject) => {
      withStore("readwrite", (store) => {
        store.transaction.oncomplete = resolve;
        var req = store.clear();
        req.onerror = function clearOnError() {
          reject("Error in asyncStorage.clear(): ", req.error.name);
        };
      }, reject);
    });
  }

  function length() {
    return new Promise((resolve, reject) => {
      var req;
      withStore("readonly", (store) => {
        store.transaction.oncomplete = function onComplete() {
          resolve(req.result);
        }
        req = store.count();
        req.onerror = function lengthOnError() {
          reject("Error in asyncStorage.length(): ", req.error.name);
        };
      }, reject);
    });
  }

  function key(n) {
    return new Promise((resolve, reject) => {
      if (n < 0) {
        resolve(null);
        return;
      }

      var req;
      withStore("readonly", (store) => {
        store.transaction.oncomplete = function onComplete() {
          var cursor = req.result;
          resolve(cursor ? cursor.key : null);
        };
        var advanced = false;
        req = store.openCursor();
        req.onsuccess = function keyOnSuccess() {
          var cursor = req.result;
          if (!cursor) {
            // this means there weren"t enough keys
            return;
          }
          if (n === 0 || advanced) {
            // Either 1) we have the first key, return it if that's what they
            // wanted, or 2) we"ve got the nth key.
            return;
          }

          // Otherwise, ask the cursor to skip ahead n records
          advanced = true;
          cursor.advance(n);
        };
        req.onerror = function keyOnError() {
          reject("Error in asyncStorage.key(): ", req.error.name);
        };
      }, reject);
    });
  }

  return {
    getItem: getItem,
    setItem: setItem,
    removeItem: removeItem,
    clear: clear,
    length: length,
    key: key
  };
}());
