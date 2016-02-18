/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var disableWorkerTest = "This test uses SpecialPowers";

var testGenerator = testSteps();

function testSteps()
{
  const fileData = "abcdefghijklmnopqrstuvwxyz";
  const fileType = "text/plain";

  const databaseName =
    ("window" in this) ? window.location.pathname : "Test";
  const objectStoreName = "foo";
  const objectStoreKey = "10";

  info("Creating temp file");

  SpecialPowers.createFiles([{data:fileData, options:{type:fileType}}], function (files) {
      testGenerator.next(files[0]);
  });

  let file = yield undefined;

  ok(file instanceof File, "Got a File object");
  is(file.size, fileData.length, "Correct size");
  is(file.type, fileType, "Correct type");

  let fileReader = new FileReader();
  fileReader.onload = grabEventAndContinueHandler;
  fileReader.readAsText(file);

  let event = yield undefined;

  is(fileReader.result, fileData, "Correct data");

  let request = indexedDB.open(databaseName, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  let db = event.target.result;
  let objectStore = db.createObjectStore(objectStoreName);
  objectStore.put(file, objectStoreKey);

  event = yield undefined;

  db = event.target.result;

  file = null;

  objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);
  objectStore.get(objectStoreKey).onsuccess = grabEventAndContinueHandler;

  event = yield undefined;

  file = event.target.result;

  ok(file instanceof File, "Got a File object");
  is(file.size, fileData.length, "Correct size");
  is(file.type, fileType, "Correct type");

  fileReader = new FileReader();
  fileReader.onload = grabEventAndContinueHandler;
  fileReader.readAsText(file);

  event = yield undefined;

  is(fileReader.result, fileData, "Correct data");

  finishTest();
  yield undefined;
}
