/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const dbName = ("window" in this) ? window.location.pathname : "test";
const dbVersion = 1;
const objName1 = "o1";
const objName2 = "o2";
const idxName1 = "i1";
const idxName2 = "i2";
const idxKeyPathProp = "idx";
const objDataProp = "data";
const objData = "1234567890";
const objDataCount = 5;
const loopCount = 100;

var testGenerator = testSteps();

function* testSteps()
{
  let req = indexedDB.open(dbName, dbVersion);
  req.onerror = errorHandler;
  req.onupgradeneeded = grabEventAndContinueHandler;
  req.onsuccess = grabEventAndContinueHandler;

  let event = yield undefined;

  is(event.type, "upgradeneeded", "Got upgradeneeded event");

  let db = event.target.result;

  let objectStore1 = db.createObjectStore(objName1);
  objectStore1.createIndex(idxName1, idxKeyPathProp);

  let objectStore2 = db.createObjectStore(objName2);
  objectStore2.createIndex(idxName2, idxKeyPathProp);

  for (let i = 0; i < objDataCount; i++) {
    var data = { };
    data[objDataProp] = objData;
    data[idxKeyPathProp] = objDataCount - i - 1;

    objectStore1.add(data, i);
    objectStore2.add(data, i);
  }

  event = yield undefined;

  is(event.type, "success", "Got success event");

  doReadOnlyTransaction(db, 0, loopCount);
  doReadWriteTransaction(db, 0, loopCount);

  // Wait for readonly and readwrite transaction loops to complete.
  yield undefined;
  yield undefined;

  finishTest();
}

function doReadOnlyTransaction(db, key, remaining)
{
  if (!remaining) {
    info("Finished all readonly transactions");
    continueToNextStep();
    return;
  }

  info("Starting readonly transaction for key " + key + ", " + remaining +
       " loops left");

  let objectStore = db.transaction(objName1, "readonly").objectStore(objName1);
  let index = objectStore.index(idxName1);

  index.openKeyCursor(key, "prev").onsuccess = function(event) {
    let cursor = event.target.result;
    ok(cursor, "Got readonly cursor");

    objectStore.get(cursor.primaryKey).onsuccess = function(event) {
      if (++key == objDataCount) {
        key = 0;
      }
      doReadOnlyTransaction(db, key, remaining - 1);
    };
  };
}

function doReadWriteTransaction(db, key, remaining)
{
  if (!remaining) {
    info("Finished all readwrite transactions");
    continueToNextStep();
    return;
  }

  info("Starting readwrite transaction for key " + key + ", " + remaining +
       " loops left");

  let objectStore = db.transaction(objName2, "readwrite").objectStore(objName2);
  objectStore.openCursor(key).onsuccess = function(event) {
    let cursor = event.target.result;
    ok(cursor, "Got readwrite cursor");

    let value = cursor.value;
    value[idxKeyPathProp]++;

    cursor.update(value).onsuccess = function(event) {
      if (++key == objDataCount) {
        key = 0;
      }
      doReadWriteTransaction(db, key, remaining - 1);
    };
  };
}
