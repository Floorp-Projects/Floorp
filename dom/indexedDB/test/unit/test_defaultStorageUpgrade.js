/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  Services.prefs.setBoolPref("dom.indexedDB.storageOption.enabled", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("dom.indexedDB.storageOption.enabled");
  });

  const openParams = [
    // This one lives in storage/default/http+++localhost
    { url: "http://localhost", dbName: "dbA", dbVersion: 1 },

    // This one lives in storage/default/http+++www.mozilla.org
    { url: "http://www.mozilla.org", dbName: "dbB", dbVersion: 1 },

    // This one lives in storage/default/http+++www.mozilla.org+8080
    { url: "http://www.mozilla.org:8080", dbName: "dbC", dbVersion: 1 },

    // This one lives in storage/default/https+++www.mozilla.org
    { url: "https://www.mozilla.org", dbName: "dbD", dbVersion: 1 },

    // This one lives in storage/default/https+++www.mozilla.org+8080
    { url: "https://www.mozilla.org:8080", dbName: "dbE", dbVersion: 1 },

    // This one lives in storage/permanent/indexeddb+++fx-devtools
    { url: "indexeddb://fx-devtools", dbName: "dbF",
      dbOptions: { version: 1, storage: "persistent" } },

    // This one lives in storage/permanent/moz-safe-about+home
    { url: "moz-safe-about:home", dbName: "dbG",
      dbOptions: { version: 1, storage: "persistent" } },

    // This one lives in storage/default/file++++Users+joe+
    { url: "file:///Users/joe/", dbName: "dbH", dbVersion: 1 },

    // This one lives in storage/default/file++++Users+joe+index.html
    { url: "file:///Users/joe/index.html", dbName: "dbI", dbVersion: 1 },

    // This one lives in storage/default/file++++c++Users+joe+
    { url: "file:///c:/Users/joe/", dbName: "dbJ", dbVersion: 1 },

    // This one lives in storage/default/file++++c++Users+joe+index.html
    { url: "file:///c:/Users/joe/index.html", dbName: "dbK", dbVersion: 1 },

    // This one lives in storage/permanent/chrome
    { dbName: "dbL", dbVersion: 1 },

    // This one lives in storage/default/http+++127.0.0.1
    { url: "http://127.0.0.1", dbName: "dbO", dbVersion: 1 },

    // This one lives in storage/default/file++++
    { url: "file:///", dbName: "dbP", dbVersion: 1 },

    // This one lives in storage/default/file++++c++
    { url: "file:///c:/", dbName: "dbQ", dbVersion: 1 },

    // This one lives in storage/default/file++++Users+joe+c+++index.html
    { url: "file:///Users/joe/c++/index.html", dbName: "dbR", dbVersion: 1 },

    // This one lives in storage/default/file++++Users+joe+c+++index.html
    { url: "file:///Users/joe/c///index.html", dbName: "dbR", dbVersion: 1 },

    // This one lives in storage/default/file++++++index.html
    { url: "file:///+/index.html", dbName: "dbS", dbVersion: 1 },

    // This one lives in storage/default/file++++++index.html
    { url: "file://///index.html", dbName: "dbS", dbVersion: 1 },

    // This one lives in storage/permanent/resource+++fx-share-addon-at-mozilla-dot-org-fx-share-addon-data
    { url: "resource://fx-share-addon-at-mozilla-dot-org-fx-share-addon-data",
      dbName: "dbU", dbOptions: { version: 1, storage: "persistent" } },

    // This one lives in storage/temporary/http+++localhost+81
    // The .metadata file was intentionally removed for this origin directory
    // to test restoring during upgrade.
    { url: "http://localhost:81", dbName: "dbV",
      dbOptions: { version: 1, storage: "temporary" } },

    // This one lives in storage/temporary/http+++localhost+82
    // The .metadata file was intentionally truncated for this origin directory
    // to test restoring during upgrade.
    { url: "http://localhost:82", dbName: "dbW",
      dbOptions: { version: 1, storage: "temporary" } },

    // This one lives in storage/temporary/http+++localhost
    { url: "http://localhost", dbName: "dbZ",
      dbOptions: { version: 1, storage: "temporary" } }
  ];

  function openDatabase(params) {
    let request;
    if ("url" in params) {
      let principal = getPrincipal(params.url);
      if ("dbVersion" in params) {
        request = indexedDB.openForPrincipal(principal, params.dbName,
                                             params.dbVersion);
      } else {
        request = indexedDB.openForPrincipal(principal, params.dbName,
                                             params.dbOptions);
      }
    } else if ("dbVersion" in params) {
      request = indexedDB.open(params.dbName, params.dbVersion);
    } else {
      request = indexedDB.open(params.dbName, params.dbOptions);
    }
    return request;
  }

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  installPackagedProfile("defaultStorageUpgrade_profile");

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
