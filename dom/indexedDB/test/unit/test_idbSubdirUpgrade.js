/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testGenerator */
var testGenerator = testSteps();

function* testSteps() {
  const openParams = [
    // This one lives in storage/default/http+++www.mozilla.org
    { url: "http://www.mozilla.org", dbName: "dbB", dbVersion: 1 },
  ];

  for (let i = 1; i <= 2; i++) {
    clearAllDatabases(continueToNextStepSync);
    yield undefined;

    installPackagedProfile("idbSubdirUpgrade" + i + "_profile");

    for (let params of openParams) {
      let request = indexedDB.openForPrincipal(
        getPrincipal(params.url),
        params.dbName,
        params.dbVersion
      );
      request.onerror = errorHandler;
      request.onupgradeneeded = unexpectedSuccessHandler;
      request.onsuccess = grabEventAndContinueHandler;
      let event = yield undefined;

      is(event.type, "success", "Correct event type");
    }

    resetAllDatabases(continueToNextStepSync);
    yield undefined;

    for (let params of openParams) {
      let request = indexedDB.openForPrincipal(
        getPrincipal(params.url),
        params.dbName,
        params.dbVersion
      );
      request.onerror = errorHandler;
      request.onupgradeneeded = unexpectedSuccessHandler;
      request.onsuccess = grabEventAndContinueHandler;
      let event = yield undefined;

      is(event.type, "success", "Correct event type");
    }
  }

  finishTest();
  yield undefined;
}
