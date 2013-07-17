/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

var self = this;

var testGenerator = testSteps();

function testSteps()
{
  const dbName = self.window ? window.location.pathname : "test_lowDiskSpace";
  const dbVersion = 1;

  const objectStoreName = "foo";
  const objectStoreOptions = { keyPath: "foo" };

  const indexName = "bar";
  const indexOptions = { unique: true };

  const dbData = [
    { foo: 0, bar: 0 },
    { foo: 1, bar: 10 },
    { foo: 2, bar: 20 },
    { foo: 3, bar: 30 },
    { foo: 4, bar: 40 },
    { foo: 5, bar: 50 },
    { foo: 6, bar: 60 },
    { foo: 7, bar: 70 },
    { foo: 8, bar: 80 },
    { foo: 9, bar: 90 }
  ];

  let lowDiskMode = false;
  function setLowDiskMode(val) {
    let data = val ? "full" : "free";

    if (val == lowDiskMode) {
      info("Low disk mode is: " + data);
    }
    else {
      info("Changing low disk mode to: " + data);
      SpecialPowers.notifyObserversInParentProcess(null, "disk-space-watcher",
                                                   data);
      lowDiskMode = val;
    }
  }

  { // Make sure opening works from the beginning.
    info("Test 1");

    setLowDiskMode(false);

    let request = indexedDB.open(dbName, dbVersion);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    is(event.type, "success", "Opened database without setting low disk mode");

    let db = event.target.result;
    db.close();
  }

  { // Make sure delete works in low disk mode.
    info("Test 2");

    setLowDiskMode(true);

    let request = indexedDB.deleteDatabase(dbName);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    is(event.type, "success", "Deleted database after setting low disk mode");
  }

  { // Make sure creating a db in low disk mode fails.
    info("Test 3");

    setLowDiskMode(true);

    let request = indexedDB.open(dbName, dbVersion);
    request.onerror = expectedErrorHandler("QuotaExceededError");
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = unexpectedSuccessHandler;
    let event = yield undefined;

    is(event.type, "error", "Didn't create new database in low disk mode");
  }

  { // Make sure opening an already-existing db in low disk mode succeeds.
    info("Test 4");

    setLowDiskMode(false);

    let request = indexedDB.open(dbName, dbVersion);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;
    let event = yield undefined;

    is(event.type, "upgradeneeded", "Upgrading database");

    let db = event.target.result;
    db.onerror = errorHandler;

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Created database");
    ok(event.target.result === db, "Got the same database");

    db.close();

    setLowDiskMode(true);

    request = indexedDB.open(dbName);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Opened existing database in low disk mode");

    db = event.target.result;
    db.close();
  }

  { // Make sure upgrading an already-existing db in low disk mode succeeds.
    info("Test 5");

    setLowDiskMode(true);

    let request = indexedDB.open(dbName, dbVersion + 1);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;

    let event = yield undefined;

    is(event.type, "upgradeneeded", "Upgrading database");

    let db = event.target.result;
    db.onerror = errorHandler;

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Created database");
    ok(event.target.result === db, "Got the same database");

    db.close();
  }

  { // Make sure creating objectStores in low disk mode fails.
    info("Test 6");

    setLowDiskMode(true);

    let request = indexedDB.open(dbName, dbVersion + 2);
    request.onerror = expectedErrorHandler("QuotaExceededError");
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;

    let event = yield undefined;

    is(event.type, "upgradeneeded", "Upgrading database");

    let db = event.target.result;
    db.onerror = errorHandler;

    let objectStore = db.createObjectStore(objectStoreName, objectStoreOptions);

    request.onupgradeneeded = unexpectedSuccessHandler;
    event = yield undefined;

    is(event.type, "error", "Failed database upgrade");
  }

  { // Make sure creating indexes in low disk mode fails.
    info("Test 7");

    setLowDiskMode(false);

    let request = indexedDB.open(dbName, dbVersion + 2);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;

    let event = yield undefined;

    is(event.type, "upgradeneeded", "Upgrading database");

    let db = event.target.result;
    db.onerror = errorHandler;

    let objectStore = db.createObjectStore(objectStoreName, objectStoreOptions);

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Upgraded database");
    ok(event.target.result === db, "Got the same database");

    db.close();

    setLowDiskMode(true);

    request = indexedDB.open(dbName, dbVersion + 3);
    request.onerror = expectedErrorHandler("QuotaExceededError");
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;
    event = yield undefined;

    is(event.type, "upgradeneeded", "Upgrading database");

    db = event.target.result;
    db.onerror = errorHandler;

    objectStore = event.target.transaction.objectStore(objectStoreName);
    let index = objectStore.createIndex(indexName, indexName, indexOptions);

    request.onupgradeneeded = unexpectedSuccessHandler;
    event = yield undefined;

    is(event.type, "error", "Failed database upgrade");
  }

  { // Make sure deleting indexes in low disk mode succeeds.
    info("Test 8");

    setLowDiskMode(false);

    let request = indexedDB.open(dbName, dbVersion + 3);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;

    let event = yield undefined;

    is(event.type, "upgradeneeded", "Upgrading database");

    let db = event.target.result;
    db.onerror = errorHandler;

    let objectStore = event.target.transaction.objectStore(objectStoreName);
    let index = objectStore.createIndex(indexName, indexName, indexOptions);

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Upgraded database");
    ok(event.target.result === db, "Got the same database");

    db.close();

    setLowDiskMode(true);

    request = indexedDB.open(dbName, dbVersion + 4);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;
    event = yield undefined;

    is(event.type, "upgradeneeded", "Upgrading database");

    db = event.target.result;
    db.onerror = errorHandler;

    objectStore = event.target.transaction.objectStore(objectStoreName);
    objectStore.deleteIndex(indexName);

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Upgraded database");
    ok(event.target.result === db, "Got the same database");

    db.close();
  }

  { // Make sure deleting objectStores in low disk mode succeeds.
    info("Test 9");

    setLowDiskMode(true);

    let request = indexedDB.open(dbName, dbVersion + 5);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;

    let event = yield undefined;

    is(event.type, "upgradeneeded", "Upgrading database");

    let db = event.target.result;
    db.onerror = errorHandler;

    db.deleteObjectStore(objectStoreName);

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Upgraded database");
    ok(event.target.result === db, "Got the same database");

    db.close();

    // Reset everything.
    indexedDB.deleteDatabase(dbName);
  }


  { // Add data that the rest of the tests will use.
    info("Adding test data");

    setLowDiskMode(false);

    let request = indexedDB.open(dbName, dbVersion);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;
    let event = yield undefined;

    is(event.type, "upgradeneeded", "Upgrading database");

    let db = event.target.result;
    db.onerror = errorHandler;

    let objectStore = db.createObjectStore(objectStoreName, objectStoreOptions);
    let index = objectStore.createIndex(indexName, indexName, indexOptions);

    for each (let data in dbData) {
      objectStore.add(data);
    }

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Upgraded database");
    ok(event.target.result === db, "Got the same database");

    db.close();
  }

  { // Make sure read operations in readonly transactions succeed in low disk
    // mode.
    info("Test 10");

    setLowDiskMode(true);

    let request = indexedDB.open(dbName, dbVersion);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    let db = event.target.result;
    db.onerror = errorHandler;

    let transaction = db.transaction(objectStoreName);
    let objectStore = transaction.objectStore(objectStoreName);
    let index = objectStore.index(indexName);

    let data = dbData[0];

    let requestCounter = new RequestCounter();

    objectStore.get(data.foo).onsuccess = requestCounter.handler();
    objectStore.mozGetAll().onsuccess = requestCounter.handler();
    objectStore.count().onsuccess = requestCounter.handler();
    index.get(data.bar).onsuccess = requestCounter.handler();
    index.mozGetAll().onsuccess = requestCounter.handler();
    index.getKey(data.bar).onsuccess = requestCounter.handler();
    index.mozGetAllKeys().onsuccess = requestCounter.handler();
    index.count().onsuccess = requestCounter.handler();

    let objectStoreDataCount = 0;

    request = objectStore.openCursor();
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        objectStoreDataCount++;
        objectStoreDataCount % 2 ? cursor.continue() : cursor.advance(1);
      }
      else {
        is(objectStoreDataCount, dbData.length, "Saw all data");
        requestCounter.decr();
      }
    };
    requestCounter.incr();

    let indexDataCount = 0;

    request = index.openCursor();
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        indexDataCount++;
        indexDataCount % 2 ? cursor.continue() : cursor.advance(1);
      }
      else {
        is(indexDataCount, dbData.length, "Saw all data");
        requestCounter.decr();
      }
    };
    requestCounter.incr();

    let indexKeyDataCount = 0;

    request = index.openCursor();
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        indexKeyDataCount++;
        indexKeyDataCount % 2 ? cursor.continue() : cursor.advance(1);
      }
      else {
        is(indexKeyDataCount, dbData.length, "Saw all data");
        requestCounter.decr();
      }
    };
    requestCounter.incr();

    // Wait for all requests.
    yield undefined;

    transaction.oncomplete = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "complete", "Transaction succeeded");

    db.close();
  }

  { // Make sure read operations in readwrite transactions succeed in low disk
    // mode.
    info("Test 11");

    setLowDiskMode(true);

    let request = indexedDB.open(dbName, dbVersion);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    let db = event.target.result;
    db.onerror = errorHandler;

    let transaction = db.transaction(objectStoreName, "readwrite");
    let objectStore = transaction.objectStore(objectStoreName);
    let index = objectStore.index(indexName);

    let data = dbData[0];

    let requestCounter = new RequestCounter();

    objectStore.get(data.foo).onsuccess = requestCounter.handler();
    objectStore.mozGetAll().onsuccess = requestCounter.handler();
    objectStore.count().onsuccess = requestCounter.handler();
    index.get(data.bar).onsuccess = requestCounter.handler();
    index.mozGetAll().onsuccess = requestCounter.handler();
    index.getKey(data.bar).onsuccess = requestCounter.handler();
    index.mozGetAllKeys().onsuccess = requestCounter.handler();
    index.count().onsuccess = requestCounter.handler();

    let objectStoreDataCount = 0;

    request = objectStore.openCursor();
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        objectStoreDataCount++;
        objectStoreDataCount % 2 ? cursor.continue() : cursor.advance(1);
      }
      else {
        is(objectStoreDataCount, dbData.length, "Saw all data");
        requestCounter.decr();
      }
    };
    requestCounter.incr();

    let indexDataCount = 0;

    request = index.openCursor();
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        indexDataCount++;
        indexDataCount % 2 ? cursor.continue() : cursor.advance(1);
      }
      else {
        is(indexDataCount, dbData.length, "Saw all data");
        requestCounter.decr();
      }
    };
    requestCounter.incr();

    let indexKeyDataCount = 0;

    request = index.openCursor();
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        indexKeyDataCount++;
        indexKeyDataCount % 2 ? cursor.continue() : cursor.advance(1);
      }
      else {
        is(indexKeyDataCount, dbData.length, "Saw all data");
        requestCounter.decr();
      }
    };
    requestCounter.incr();

    // Wait for all requests.
    yield undefined;

    transaction.oncomplete = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "complete", "Transaction succeeded");

    db.close();
  }

  { // Make sure write operations in readwrite transactions fail in low disk
    // mode.
    info("Test 12");

    setLowDiskMode(true);

    let request = indexedDB.open(dbName, dbVersion);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    let db = event.target.result;
    db.onerror = errorHandler;

    let transaction = db.transaction(objectStoreName, "readwrite");
    let objectStore = transaction.objectStore(objectStoreName);
    let index = objectStore.index(indexName);

    let data = dbData[0];
    let newData = { foo: 999, bar: 999 };

    let requestCounter = new RequestCounter();

    objectStore.add(newData).onerror = requestCounter.errorHandler();
    objectStore.put(newData).onerror = requestCounter.errorHandler();

    objectStore.get(data.foo).onsuccess = requestCounter.handler();
    objectStore.mozGetAll().onsuccess = requestCounter.handler();
    objectStore.count().onsuccess = requestCounter.handler();
    index.get(data.bar).onsuccess = requestCounter.handler();
    index.mozGetAll().onsuccess = requestCounter.handler();
    index.getKey(data.bar).onsuccess = requestCounter.handler();
    index.mozGetAllKeys().onsuccess = requestCounter.handler();
    index.count().onsuccess = requestCounter.handler();

    let objectStoreDataCount = 0;

    request = objectStore.openCursor();
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        objectStoreDataCount++;
        cursor.update(cursor.value).onerror = requestCounter.errorHandler();
        objectStoreDataCount % 2 ? cursor.continue() : cursor.advance(1);
      }
      else {
        is(objectStoreDataCount, dbData.length, "Saw all data");
        requestCounter.decr();
      }
    };
    requestCounter.incr();

    let indexDataCount = 0;

    request = index.openCursor();
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        indexDataCount++;
        cursor.update(cursor.value).onerror = requestCounter.errorHandler();
        indexDataCount % 2 ? cursor.continue() : cursor.advance(1);
      }
      else {
        is(indexDataCount, dbData.length, "Saw all data");
        requestCounter.decr();
      }
    };
    requestCounter.incr();

    let indexKeyDataCount = 0;

    request = index.openCursor();
    request.onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        indexKeyDataCount++;
        cursor.update(cursor.value).onerror = requestCounter.errorHandler();
        indexKeyDataCount % 2 ? cursor.continue() : cursor.advance(1);
      }
      else {
        is(indexKeyDataCount, dbData.length, "Saw all data");
        requestCounter.decr();
      }
    };
    requestCounter.incr();

    // Wait for all requests.
    yield undefined;

    transaction.oncomplete = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "complete", "Transaction succeeded");

    db.close();
  }

  { // Make sure deleting operations in readwrite transactions succeed in low
    // disk mode.
    info("Test 13");

    setLowDiskMode(true);

    let request = indexedDB.open(dbName, dbVersion);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    let db = event.target.result;
    db.onerror = errorHandler;

    let transaction = db.transaction(objectStoreName, "readwrite");
    let objectStore = transaction.objectStore(objectStoreName);
    let index = objectStore.index(indexName);

    let dataIndex = 0;
    let data = dbData[dataIndex++];

    let requestCounter = new RequestCounter();

    objectStore.delete(data.foo).onsuccess = requestCounter.handler();

    objectStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        cursor.delete().onsuccess = requestCounter.handler();
      }
      requestCounter.decr();
    };
    requestCounter.incr();

    index.openCursor(null, "prev").onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        cursor.delete().onsuccess = requestCounter.handler();
      }
      requestCounter.decr();
    };
    requestCounter.incr();

    yield undefined;

    objectStore.count().onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.target.result, dbData.length - 3, "Actually deleted something");

    objectStore.clear();
    objectStore.count().onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.target.result, 0, "Actually cleared");

    transaction.oncomplete = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "complete", "Transaction succeeded");

    db.close();
  }

  finishTest();
  yield undefined;
}

function RequestCounter(expectedType) {
  this._counter = 0;
}
RequestCounter.prototype = {
  incr: function() {
    this._counter++;
  },

  decr: function() {
    if (!--this._counter) {
      continueToNextStepSync();
    }
  },

  handler: function(type, preventDefault) {
    this.incr();
    return function(event) {
      is(event.type, type || "success", "Correct type");
      this.decr();
    }.bind(this);
  },

  errorHandler: function(eventType, errorName) {
    this.incr();
    return function(event) {
      is(event.type, eventType || "error", "Correct type");
      is(event.target.error.name, errorName || "QuotaExceededError",
          "Correct error name");
      event.preventDefault();
      event.stopPropagation();
      this.decr();
    }.bind(this);
  }
};
