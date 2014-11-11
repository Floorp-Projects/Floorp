/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let testGenerator = testSteps();

function testSteps()
{
  const dbName = ("window" in this) ? window.location.pathname : "test";
  const objName1 = "foo";
  const objName2 = "bar";
  const data1 = "1234567890";
  const data2 = "0987654321";
  const dataCount = 500;

  let request = indexedDB.open(dbName, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;

  let event = yield undefined;

  is(event.type, "upgradeneeded", "Got upgradeneeded");

  request.onupgradeneeded = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;

  let db = request.result;

  let objectStore1 = db.createObjectStore(objName1, { autoIncrement: true });
  let objectStore2 = db.createObjectStore(objName2, { autoIncrement: true });

  info("Created object stores, adding data");

  for (let i = 0; i < dataCount; i++) {
    objectStore1.add(data1);
    objectStore2.add(data2);
  }

  info("Done adding data");

  event = yield undefined;

  is(event.type, "success", "Got success");

  let readResult = null;
  let readError = null;
  let writeAborted = false;

  info("Creating readwrite transaction");

  objectStore1 = db.transaction(objName1, "readwrite").objectStore(objName1);
  objectStore1.openCursor().onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  let cursor = event.target.result;
  is(cursor.value, data1, "Got correct data for readwrite transaction");

  info("Modifying object store on readwrite transaction");

  cursor.update(data2);
  cursor.continue();

  event = yield undefined;

  info("Done modifying object store on readwrite transaction, creating " +
       "readonly transaction");

  objectStore2 = db.transaction(objName2, "readonly").objectStore(objName2);
  request = objectStore2.getAll();
  request.onsuccess = function(event) {
    readResult = event.target.result;
    is(readResult.length,
       dataCount,
       "Got correct number of results on readonly transaction");
    for (let i = 0; i < readResult.length; i++) {
      is(readResult[i], data2, "Got correct data for readonly transaction");
    }
    if (writeAborted) {
      continueToNextStep();
    }
  };
  request.onerror = function(event) {
    readResult = null;
    readError = event.target.error;

    ok(false, "Got read error: " + readError.name);
    event.preventDefault();

    if (writeAborted) {
      continueToNextStep();
    }
  }

  cursor = event.target.result;
  is(cursor.value, data1, "Got correct data for readwrite transaction");

  info("Aborting readwrite transaction");

  cursor.source.transaction.abort();
  writeAborted = true;

  if (!readError && !readResult) {
    info("Waiting for readonly transaction to complete");
    yield undefined;
  }

  ok(readResult, "Got result from readonly transaction");
  is(readError, null, "No read error");
  is(writeAborted, true, "Aborted readwrite transaction");

  finishTest();
  yield undefined;
}
