/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testGenerator */
var testGenerator = testSteps();

function* testSteps() {
  const openParams = [
    // This one lives in storage/default/http+++localhost+81
    {
      url: "http://localhost:81",
      dbName: "dbC",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+82
    {
      url: "http://localhost:82",
      dbName: "dbD",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+83
    {
      url: "http://localhost:83",
      dbName: "dbE",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+84
    {
      url: "http://localhost:84",
      dbName: "dbF",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+85
    {
      url: "http://localhost:85",
      dbName: "dbG",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+86
    {
      url: "http://localhost:86",
      dbName: "dbH",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+87
    {
      url: "http://localhost:87",
      dbName: "dbI",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+88
    {
      url: "http://localhost:88",
      dbName: "dbJ",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+89
    {
      url: "http://localhost:89",
      dbName: "dbK",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+90
    {
      url: "http://localhost:90",
      dbName: "dbL",
      dbVersion: 1,
    },
  ];

  function openDatabase(params) {
    let request;
    if ("url" in params) {
      let uri = Services.io.newURI(params.url);
      let principal = Services.scriptSecurityManager.createContentPrincipal(
        uri,
        {}
      );
      request = indexedDB.openForPrincipal(
        principal,
        params.dbName,
        params.dbVersion
      );
    } else {
      request = indexedDB.open(params.dbName, params.dbVersion);
    }
    return request;
  }

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  installPackagedProfile("metadataRestore_profile");

  for (let params of openParams) {
    let request = openDatabase(params);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    is(event.type, "success", "Correct event type");
  }

  resetAllDatabases(continueToNextStepSync);
  yield undefined;

  for (let params of openParams) {
    let request = openDatabase(params);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    is(event.type, "success", "Correct event type");
  }

  finishTest();
  yield undefined;
}
