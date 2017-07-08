var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname :
               "test_storage_manager_estimate.js";
  const objectStoreName = "storagesManager";
  const arraySize = 1e6;

  ok("estimate" in navigator.storage, "Has estimate function");
  is(typeof navigator.storage.estimate, "function", "estimate is function");
  ok(navigator.storage.estimate() instanceof Promise,
     "estimate() method exists and returns a Promise");

  navigator.storage.estimate().then(estimation => {
    testGenerator.next(estimation.usage);
  });

  let before = yield undefined;

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = continueToNextStep;
  let event = yield undefined;

  let db = event.target.result;
  db.onerror = errorHandler;

  let objectStore = db.createObjectStore(objectStoreName, { });
  yield undefined;

  navigator.storage.estimate().then(estimation => {
    testGenerator.next(estimation.usage);
  });
  let usageAfterCreate = yield undefined;
  ok(usageAfterCreate > before, "estimated usage must increase after createObjectStore");

  let txn = db.transaction(objectStoreName, "readwrite");
  objectStore = txn.objectStore(objectStoreName);
  objectStore.put(new Uint8Array(arraySize), "k");
  txn.oncomplete = continueToNextStep;
  txn.onabort = errorHandler;
  txn.onerror = errorHandler;
  event = yield undefined;

  navigator.storage.estimate().then(estimation => {
    testGenerator.next(estimation.usage);
  });
  let usageAfterPut = yield undefined;
  ok(usageAfterPut > usageAfterCreate, "estimated usage must increase after putting large object");
  db.close();

  finishTest();
}

function setup()
{
  SpecialPowers.pushPrefEnv({
    "set": [["dom.storageManager.enabled", true]]
  }, runTest);
}
