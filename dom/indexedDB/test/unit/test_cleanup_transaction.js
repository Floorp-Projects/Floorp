/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var disableWorkerTest = "Need a way to set temporary prefs from a worker";

var testGenerator = testSteps();

function testSteps()
{
  const spec = "http://foo.com";
  const name =
    this.window ? window.location.pathname : "test_quotaExceeded_recovery";
  const objectStoreName = "foo";

  // We want 32 MB database, but there's the group limit so we need to
  // multiply by 5.
  const tempStorageLimitKB = 32 * 1024 * 5;

  // Store in 1 MB chunks.
  const dataSize = 1024 * 1024;

  for (let blobs of [false, true]) {
    setTemporaryStorageLimit(tempStorageLimitKB);

    clearAllDatabases(continueToNextStepSync);
    yield undefined;

    info("Opening database");

    let request = indexedDB.openForPrincipal(getPrincipal(spec), name);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;;
    request.onsuccess = unexpectedSuccessHandler;

    yield undefined;

    // upgradeneeded
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;

    info("Creating objectStore");

    request.result.createObjectStore(objectStoreName);

    yield undefined;

    // success
    let db = request.result;
    db.onerror = errorHandler;

    ok(true, "Adding data until quota is reached");

    let obj = {
      name: "foo"
    }

    if (!blobs) {
      obj.data = getRandomView(dataSize);
    }

    let i = 1;
    let j = 1;
    while (true) {
      if (blobs) {
        obj.data = getBlob(getView(dataSize));
      }

      let trans = db.transaction(objectStoreName, "readwrite");
      request = trans.objectStore(objectStoreName).add(obj, i);
      request.onerror = function(event)
      {
        event.stopPropagation();
      }

      trans.oncomplete = function(event) {
        i++;
        j++;
        testGenerator.send(true);
      }
      trans.onabort = function(event) {
        is(trans.error.name, "QuotaExceededError", "Reached quota limit");
        testGenerator.send(false);
      }

      let completeFired = yield undefined;
      if (completeFired) {
        ok(true, "Got complete event");
      } else {
        ok(true, "Got abort event");

        if (j == 1) {
          // Plain cleanup transaction (just vacuuming and checkpointing)
          // couldn't shrink database any further.
          break;
        }

        j = 1;

        trans = db.transaction(objectStoreName, "cleanup");
        trans.onabort = unexpectedSuccessHandler;;
        trans.oncomplete = grabEventAndContinueHandler;

        yield undefined;
      }
    }

    info("Reopening database");

    db.close();

    request = indexedDB.openForPrincipal(getPrincipal(spec), name);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;

    yield undefined;

    db = request.result;
    db.onerror = errorHandler;

    info("Deleting some data")

    let trans = db.transaction(objectStoreName, "cleanup");
    trans.objectStore(objectStoreName).delete(1);

    trans.onabort = unexpectedSuccessHandler;;
    trans.oncomplete = grabEventAndContinueHandler;

    yield undefined;

    info("Adding data again")

    trans = db.transaction(objectStoreName, "readwrite");
    trans.objectStore(objectStoreName).add(obj, 1);

    trans.onabort = unexpectedSuccessHandler;
    trans.oncomplete = grabEventAndContinueHandler;

    yield undefined;

    info("Deleting database");

    db.close();

    request = indexedDB.deleteForPrincipal(getPrincipal(spec), name);
    request.onerror = errorHandler;
    request.onsuccess = grabEventAndContinueHandler;

    yield undefined;
  }

  finishTest();
  yield undefined;
}
