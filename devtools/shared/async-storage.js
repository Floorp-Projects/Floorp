/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

"use strict";

const DBNAME = "devtools-async-storage";
const DBVERSION = 1;
const STORENAME = "keyvaluepairs";
var db = null;

function withStore(type, onsuccess, onerror) {
  if (db) {
    const transaction = db.transaction(STORENAME, type);
    const store = transaction.objectStore(STORENAME);
    onsuccess(store);
  } else {
    const openreq = indexedDB.open(DBNAME, DBVERSION);
    openreq.onerror = function withStoreOnError() {
      onerror();
    };
    openreq.onupgradeneeded = function withStoreOnUpgradeNeeded() {
      // First time setup: create an empty object store
      openreq.result.createObjectStore(STORENAME);
    };
    openreq.onsuccess = function withStoreOnSuccess() {
      db = openreq.result;
      const transaction = db.transaction(STORENAME, type);
      const store = transaction.objectStore(STORENAME);
      onsuccess(store);
    };
  }
}

function getItem(itemKey) {
  return new Promise((resolve, reject) => {
    let req;
    withStore(
      "readonly",
      store => {
        store.transaction.oncomplete = function onComplete() {
          let value = req.result;
          if (value === undefined) {
            value = null;
          }
          resolve(value);
        };
        req = store.get(itemKey);
        req.onerror = function getItemOnError() {
          console.error("Error in asyncStorage.getItem():", req.error.name);
          reject(req.error);
        };
      },
      reject
    );
  });
}

function setItem(itemKey, value) {
  return new Promise((resolve, reject) => {
    withStore(
      "readwrite",
      store => {
        store.transaction.oncomplete = resolve;
        const req = store.put(value, itemKey);
        req.onerror = function setItemOnError() {
          console.error("Error in asyncStorage.setItem():", req.error.name);
          reject(req.error);
        };
      },
      reject
    );
  });
}

function removeItem(itemKey) {
  return new Promise((resolve, reject) => {
    withStore(
      "readwrite",
      store => {
        store.transaction.oncomplete = resolve;
        const req = store.delete(itemKey);
        req.onerror = function removeItemOnError() {
          console.error("Error in asyncStorage.removeItem():", req.error.name);
          reject(req.error);
        };
      },
      reject
    );
  });
}

function clear() {
  return new Promise((resolve, reject) => {
    withStore(
      "readwrite",
      store => {
        store.transaction.oncomplete = resolve;
        const req = store.clear();
        req.onerror = function clearOnError() {
          console.error("Error in asyncStorage.clear():", req.error.name);
          reject(req.error);
        };
      },
      reject
    );
  });
}

function length() {
  return new Promise((resolve, reject) => {
    let req;
    withStore(
      "readonly",
      store => {
        store.transaction.oncomplete = function onComplete() {
          resolve(req.result);
        };
        req = store.count();
        req.onerror = function lengthOnError() {
          console.error("Error in asyncStorage.length():", req.error.name);
          reject(req.error.name);
        };
      },
      reject
    );
  });
}

function key(n) {
  return new Promise((resolve, reject) => {
    if (n < 0) {
      resolve(null);
      return;
    }

    let req;
    withStore(
      "readonly",
      store => {
        store.transaction.oncomplete = function onComplete() {
          const cursor = req.result;
          resolve(cursor ? cursor.key : null);
        };
        let advanced = false;
        req = store.openCursor();
        req.onsuccess = function keyOnSuccess() {
          const cursor = req.result;
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
          console.error("Error in asyncStorage.key():", req.error.name);
          reject(req.error);
        };
      },
      reject
    );
  });
}

exports.getItem = getItem;
exports.setItem = setItem;
exports.removeItem = removeItem;
exports.clear = clear;
exports.length = length;
exports.key = key;
