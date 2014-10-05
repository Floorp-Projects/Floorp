/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Make it possible to load LoopStorage.jsm in xpcshell tests
try {
  Cu.importGlobalProperties(["indexedDB"]);
} catch (ex) {
  // don't write this is out in xpcshell, since it's expected there
  if (typeof window !== 'undefined' && "console" in window) {
    console.log("Failed to import indexedDB; if this isn't a unit test," +
                " something is wrong", ex);
  }
}

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "eventEmitter", function() {
  const {EventEmitter} = Cu.import("resource://gre/modules/devtools/event-emitter.js", {});
  return new EventEmitter();
});

this.EXPORTED_SYMBOLS = ["LoopStorage"];

const kDatabaseName = "loop";
const kDatabaseVersion = 1;

let gWaitForOpenCallbacks = new Set();
let gDatabase = null;
let gClosed = false;

/**
 * Properly shut the database instance down. This is done on application shutdown.
 */
const closeDatabase = function() {
  Services.obs.removeObserver(closeDatabase, "quit-application");
  if (!gDatabase) {
    return;
  }
  gDatabase.close();
  gDatabase = null;
  gClosed = true;
};

/**
 * Open a connection to the IndexedDB database.
 * This function is different than IndexedDBHelper.jsm provides, as it ensures
 * only one connection is open during the lifetime of this API. Callbacks are
 * queued when a connection attempt is in progress and are invoked once the
 * connection is established.
 *
 * @param {Function} onOpen Callback to be invoked once a database connection is
 *                          established. It takes an Error object as first argument
 *                          and the database connection object as second argument,
 *                          if successful.
 */
const ensureDatabaseOpen = function(onOpen) {
  if (gClosed) {
    onOpen(new Error("Database already closed"));
    return;
  }

  if (gDatabase) {
    onOpen(null, gDatabase);
    return;
  }

  if (!gWaitForOpenCallbacks.has(onOpen)) {
    gWaitForOpenCallbacks.add(onOpen);

    if (gWaitForOpenCallbacks.size !== 1) {
      return;
    }
  }

  let invokeCallbacks = err => {
    for (let callback of gWaitForOpenCallbacks) {
      callback(err, gDatabase);
    }
    gWaitForOpenCallbacks.clear();
  };

  let openRequest = indexedDB.open(kDatabaseName, kDatabaseVersion);

  openRequest.onblocked = function(event) {
    invokeCallbacks(new Error("Database cannot be upgraded cause in use: " + event.target.error));
  };

  openRequest.onerror = function(event) {
    // Try to delete the old database so that we can start this process over
    // next time.
    indexedDB.deleteDatabase(kDatabaseName);
    invokeCallbacks(new Error("Error while opening database: " + event.target.errorCode));
  };

  openRequest.onupgradeneeded = function(event) {
    let db = event.target.result;
    eventEmitter.emit("upgrade", db, event.oldVersion, kDatabaseVersion);
  };

  openRequest.onsuccess = function(event) {
    gDatabase = event.target.result;
    invokeCallbacks();
    // Close the database instance properly on application shutdown.
    Services.obs.addObserver(closeDatabase, "quit-application", false);
  };
};

/**
 * Start a transaction on the loop database and return it.
 *
 * @param {String}   store    Name of the object store to start a transaction on
 * @param {Function} callback Callback to be invoked once a database connection
 *                            is established and a transaction can be started.
 *                            It takes an Error object as first argument and the
 *                            transaction object as second argument.
 * @param {String}   mode     Mode of the transaction. May be 'readonly' or 'readwrite'
 *
 * @note we can't use a Promise here, as they are resolved after a spin of the
 *       event loop; the transaction will have finished by then and no operations
 *       are possible anymore, yielding an error.
 */
const getTransaction = function(store, callback, mode) {
  ensureDatabaseOpen((err, db) => {
    if (err) {
      callback(err);
      return;
    }

    let trans;
    try {
      trans = db.transaction(store, mode);
    } catch(ex) {
      callback(ex);
      return;
    }
    callback(null, trans);
  });
};

/**
 * Start a transaction on the loop database and return the requested store.
 *
 * @param {String}   store    Name of the object store to retrieve
 * @param {Function} callback Callback to be invoked once a database connection
 *                            is established and a transaction can be started.
 *                            It takes an Error object as first argument and the
 *                            store object as second argument.
 * @param {String}   mode     Mode of the transaction. May be 'readonly' or 'readwrite'
 *
 * @note we can't use a Promise here, as they are resolved after a spin of the
 *       event loop; the transaction will have finished by then and no operations
 *       are possible anymore, yielding an error.
 */
const getStore = function(store, callback, mode) {
  getTransaction(store, (err, trans) => {
    if (err) {
      callback(err);
      return;
    }

    callback(null, trans.objectStore(store));
  }, mode);
};

/**
 * Public Loop Storage API.
 *
 * Since IndexedDB transaction can not stand a spin of the event loop _before_
 * using a IDBTransaction object, we can't use Promise.jsm promises. Therefore
 * LoopStorage provides two async helper functions, `asyncForEach` and `asyncParallel`.
 *
 * LoopStorage implements the EventEmitter interface by exposing two methods, `on`
 * and `off`, to subscribe to events.
 * At this point only the `upgrade` event will be emitted. This happens when the
 * database is loaded in memory and consumers will be able to change its structure.
 */
this.LoopStorage = Object.freeze({
  /**
   * Open a connection to the IndexedDB database and return the database object.
   *
   * @param {Function} callback Callback to be invoked once a database connection
   *                            is established. It takes an Error object as first
   *                            argument and the database connection object as
   *                            second argument, if successful.
   */
  getSingleton: function(callback) {
    ensureDatabaseOpen(callback);
  },

  /**
   * Start a transaction on the loop database and return it.
   * If only two arguments are passed, the default mode will be assumed and the
   * second argument is assumed to be a callback.
   *
   * @param {String}   store    Name of the object store to start a transaction on
   * @param {Function} callback Callback to be invoked once a database connection
   *                            is established and a transaction can be started.
   *                            It takes an Error object as first argument and the
   *                            transaction object as second argument.
   * @param {String}   mode     Mode of the transaction. May be 'readonly' or 'readwrite'
   *
   * @note we can't use a Promise here, as they are resolved after a spin of the
   *       event loop; the transaction will have finished by then and no operations
   *       are possible anymore, yielding an error.
   */
  getTransaction: function(store, callback, mode = "readonly") {
    getTransaction(store, callback, mode);
  },

  /**
   * Start a transaction on the loop database and return the requested store.
   * If only two arguments are passed, the default mode will be assumed and the
   * second argument is assumed to be a callback.
   *
   * @param {String}   store    Name of the object store to retrieve
   * @param {Function} callback Callback to be invoked once a database connection
   *                            is established and a transaction can be started.
   *                            It takes an Error object as first argument and the
   *                            store object as second argument.
   * @param {String}   mode     Mode of the transaction. May be 'readonly' or 'readwrite'
   *
   * @note we can't use a Promise here, as they are resolved after a spin of the
   *       event loop; the transaction will have finished by then and no operations
   *       are possible anymore, yielding an error.
   */
  getStore: function(store, callback, mode = "readonly") {
    getStore(store, callback, mode);
  },

  /**
   * Perform an async function in serial on each of the list items and call a
   * callback Function when all list items are done.
   * IMPORTANT: only use this iteration method if you are sure that the operations
   * performed in `onItem` are guaranteed to be async in the success case.
   *
   * @param {Array}    list     Non-empty list of items to iterate
   * @param {Function} onItem   Callback to invoke for each item in the list. It
   *                            takes the item is first argument and a callback
   *                            function as second, which is to be invoked once
   *                            the consumer is done with its async operation. If
   *                            an error is passed as the first argument to this
   *                            callback function, the iteration will stop and
   *                            `onDone` callback will be invoked with that error.
   * @param {callback} onDone   Callback to invoke when the list is completed or
   *                            on error. It takes an Error object as first
   *                            argument.
   */
  asyncForEach: function(list, onItem, onDone) {
    let i = 0;
    let len = list.length;

    if (!len) {
      onDone(new Error("Argument error: empty list"));
      return;
    }

    onItem(list[i], function handler(err) {
      if (err) {
        onDone(err);
        return;
      }

      i++;
      if (i < len) {
        onItem(list[i], handler, i);
      } else {
        onDone();
      }
    }, i);
  },

  /**
   * Perform an async function in parallel on each of the list items and call a
   * callback Function when all list items are done.
   * IMPORTANT: only use this iteration method if you are sure that the operations
   * performed in `onItem` are guaranteed to be async in the success case.
   *
   * @param {Array}    list     Non-empty list of items to iterate
   * @param {Function} onItem   Callback to invoke for each item in the list. It
   *                            takes the item is first argument and a callback
   *                            function as second, which is to be invoked once
   *                            the consumer is done with its async operation. If
   *                            an error is passed as the first argument to this
   *                            callback function, the iteration will stop and
   *                            `onDone` callback will be invoked with that error.
   * @param {callback} onDone   Callback to invoke when the list is completed or
   *                            on error. It takes an Error object as first
   *                            argument.
   */
  asyncParallel: function(list, onItem, onDone) {
    let i = 0;
    let done = 0;
    let callbackCalled = false;
    let len = list.length;

    if (!len) {
      onDone(new Error("Argument error: empty list"));
      return;
    }

    for (; i < len; ++i) {
      onItem(list[i], function handler(err) {
        if (callbackCalled) {
          return;
        }

        if (err) {
          onDone(err);
          callbackCalled = true;
          return;
        }

        if (++done === len) {
          onDone();
          callbackCalled = true;
        }
      }, i);
    }
  },

  on: (...params) => eventEmitter.on(...params),

  off: (...params) => eventEmitter.off(...params)
});
