/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var disableWorkerTest = "Need a way to set temporary prefs from a worker";

var testGenerator = testSteps();

function* testSteps() {
  const name = this.window
    ? window.location.pathname
    : "test_view_put_get_values.js";

  const objectStoreName = "Views";

  const viewData = { key: 1, view: getRandomView(100000) };

  const tests = [
    {
      external: false,
      preprocessing: false,
    },
    {
      external: true,
      preprocessing: false,
    },
    {
      external: true,
      preprocessing: true,
    },
  ];

  for (let test of tests) {
    if (test.external) {
      if (this.window) {
        info("Setting data threshold pref");

        SpecialPowers.pushPrefEnv(
          { set: [["dom.indexedDB.dataThreshold", 0]] },
          continueToNextStep
        );
        yield undefined;
      } else {
        setDataThreshold(0);
      }
    }

    if (test.preprocessing) {
      if (this.window) {
        info("Setting preprocessing pref");

        SpecialPowers.pushPrefEnv(
          { set: [["dom.indexedDB.preprocessing", true]] },
          continueToNextStep
        );
        yield undefined;
      } else {
        enablePreprocessing();
      }
    }

    info("Opening database");

    let request = indexedDB.open(name);
    request.onerror = errorHandler;
    request.onupgradeneeded = continueToNextStepSync;
    request.onsuccess = unexpectedSuccessHandler;
    yield undefined;

    // upgradeneeded
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = continueToNextStepSync;

    info("Creating objectStore");

    request.result.createObjectStore(objectStoreName);

    yield undefined;

    // success
    let db = request.result;
    db.onerror = errorHandler;

    info("Storing view");

    let objectStore = db
      .transaction([objectStoreName], "readwrite")
      .objectStore(objectStoreName);
    request = objectStore.add(viewData.view, viewData.key);
    request.onsuccess = continueToNextStepSync;
    yield undefined;

    is(request.result, viewData.key, "Got correct key");

    info("Getting view");

    request = objectStore.get(viewData.key);
    request.onsuccess = continueToNextStepSync;
    yield undefined;

    verifyView(request.result, viewData.view);
    yield undefined;

    info("Getting view in new transaction");

    request = db
      .transaction([objectStoreName])
      .objectStore(objectStoreName)
      .get(viewData.key);
    request.onsuccess = continueToNextStepSync;
    yield undefined;

    verifyView(request.result, viewData.view);
    yield undefined;

    getCurrentUsage(grabFileUsageAndContinueHandler);
    let fileUsage = yield undefined;

    if (test.external) {
      ok(fileUsage > 0, "File usage is not zero");
    } else {
      ok(fileUsage == 0, "File usage is zero");
    }

    db.close();

    request = indexedDB.deleteDatabase(name);
    request.onerror = errorHandler;
    request.onsuccess = continueToNextStepSync;
    yield undefined;

    if (this.window) {
      info("Resetting prefs");

      SpecialPowers.popPrefEnv(continueToNextStep);
      yield undefined;
    } else {
      if (test.external) {
        resetDataThreshold();
      }

      if (test.preprocessing) {
        resetPreprocessing();
      }
    }
  }

  finishTest();
}
