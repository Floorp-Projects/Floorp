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

  // We want 8 MB database on Android and 32 MB database on other platforms.
  const groupLimitMB = mozinfo.os == "android" ? 8 : 32;

  // The group limit is calculated as 20% of the global temporary storage limit.
  const tempStorageLimitKB = groupLimitMB * 5 * 1024;

  // Store in 1 MB chunks.
  const dataSize = 1024 * 1024;

  const maxIter = 10;

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

    request.result.createObjectStore(objectStoreName, { autoIncrement: true });

    yield undefined;

    // success
    let db = request.result;
    db.onerror = errorHandler;

    ok(true, "Filling database");

    let obj = {
      name: "foo"
    }

    if (!blobs) {
      obj.data = getRandomView(dataSize);
    }

    let iter = 1;
    let i = 1;
    let j = 1;
    while (true) {
      if (blobs) {
        obj.data = getBlob(getView(dataSize));
      }

      let trans = db.transaction(objectStoreName, "readwrite");
      request = trans.objectStore(objectStoreName).add(obj);
      request.onerror = function(event)
      {
        event.stopPropagation();
      }

      trans.oncomplete = function(event) {
        if (iter == 1) {
          i++;
        }
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
        continue;
      }

      ok(true, "Got abort event");

      if (iter++ == maxIter) {
        break;
      }

      if (iter > 1) {
        ok(i == j, "Recycled entire database");
        j = 1;
      }

      trans = db.transaction(objectStoreName, "readwrite");

      // Don't use a cursor for deleting stored blobs (Cursors prolong live
      // of stored files since each record must be fetched from the database
      // first which creates a memory reference to the stored blob.)
      if (blobs) {
        request = trans.objectStore(objectStoreName).clear();
      } else {
        request = trans.objectStore(objectStoreName).openCursor();
        request.onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            cursor.delete();
            cursor.continue();
          }
        }
      }

      trans.onabort = unexpectedSuccessHandler;;
      trans.oncomplete = grabEventAndContinueHandler;

      yield undefined;
    }
  }

  finishTest();
  yield undefined;
}
