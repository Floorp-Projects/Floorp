/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var disableWorkerTest = "Need a way to set temporary prefs from a worker";

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ?
    window.location.pathname : "test_maximal_serialized_object_size.js";
  const megaBytes = 1024 * 1024;
  const kMessageOverhead = 1; // in MB
  const kMaxIpcMessageSize = 20; // in MB
  const kMaxIdbMessageSize = kMaxIpcMessageSize - kMessageOverhead;

  let chunks = new Array(kMaxIdbMessageSize);
  for (let i = 0; i < kMaxIdbMessageSize; i++) {
    chunks[i] = new ArrayBuffer(1 * megaBytes);
  }

  if (this.window) {
    SpecialPowers.pushPrefEnv(
      { "set": [["dom.indexedDB.maxSerializedMsgSize",
                 kMaxIpcMessageSize * megaBytes ]]
      },
      continueToNextStep
    );
    yield undefined;
  } else {
    setMaxSerializedMsgSize(kMaxIpcMessageSize * megaBytes);
  }

  let openRequest = indexedDB.open(name, 1);
  openRequest.onerror = errorHandler;
  openRequest.onupgradeneeded = grabEventAndContinueHandler;
  openRequest.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;

  let db = event.target.result;
  let txn = event.target.transaction;

  is(db.objectStoreNames.length, 0, "Correct objectStoreNames list");

  let objectStore = db.createObjectStore("test store", { keyPath: "id" });
  is(db.objectStoreNames.length, 1, "Correct objectStoreNames list");
  is(db.objectStoreNames.item(0), objectStore.name, "Correct object store name");

  function testTooLargeError(aOperation, aObject) {
    try {
      objectStore[aOperation](aObject).onerror = errorHandler;
      ok(false, "UnknownError is expected to be thrown!");
    } catch (e) {
      ok(e instanceof DOMException, "got a DOM exception");
      is(e.name, "UnknownError", "correct error");
      ok(!!e.message, "Error message: " + e.message);
      ok(e.message.startsWith("The serialized value is too large"),
         "Correct error message prefix.");
    }
  }

  info("Verify IDBObjectStore.add() - object is too large");
  testTooLargeError("add", { id: 1, data: chunks });

  info("Verify IDBObjectStore.add() - object size is closed to the maximal size.");
  chunks.length = chunks.length - 1;
  let request = objectStore.add({ id: 1, data: chunks });
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  yield undefined;

  info("Verify IDBObjectStore.add() - object key is too large");
  chunks.length = 10;
  testTooLargeError("add", { id: chunks });

  objectStore.createIndex("index name", "index");
  ok(objectStore.index("index name"), "Index created.");

  info("Verify IDBObjectStore.add() - index key is too large");
  testTooLargeError("add", { id: 2, index: chunks });

  info("Verify IDBObjectStore.add() - object key and index key are too large");
  let indexChunks = chunks.splice(0, 5);
  testTooLargeError("add", { id: chunks, index: indexChunks });

  openRequest.onsuccess = continueToNextStep;
  yield undefined;

  db.close();

  finishTest();
  yield undefined;
}
