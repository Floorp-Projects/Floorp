/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
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

    // This one lives in storage/default/1007+f+app+++system.gaiamobile.org
    { appId: 1007, inMozBrowser: false, url: "app://system.gaiamobile.org",
      dbName: "dbM", dbVersion: 1 },

    // This one lives in storage/default/1007+t+https+++developer.cdn.mozilla.net
    { appId: 1007, inMozBrowser: true, url: "https://developer.cdn.mozilla.net",
      dbName: "dbN", dbVersion: 1 },

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

    // This one lives in storage/temporary/http+++localhost
    { url: "http://localhost", dbName: "dbZ",
      dbOptions: { version: 1, storage: "temporary" } }
  ];

  let ios = SpecialPowers.Cc["@mozilla.org/network/io-service;1"]
                         .getService(SpecialPowers.Ci.nsIIOService);

  let ssm = SpecialPowers.Cc["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(SpecialPowers.Ci.nsIScriptSecurityManager);

  function openDatabase(params) {
    let request;
    if ("url" in params) {
      let uri = ios.newURI(params.url, null, null);
      let principal =
        ssm.createCodebasePrincipal(uri,
                                    {appId: params.appId || ssm.NO_APPID,
                                     inBrowser: params.inMozBrowser});
      if ("dbVersion" in params) {
        request = indexedDB.openForPrincipal(principal, params.dbName,
                                             params.dbVersion);
      } else {
        request = indexedDB.openForPrincipal(principal, params.dbName,
                                             params.dbOptions);
      }
    } else {
      if ("dbVersion" in params) {
        request = indexedDB.open(params.dbName, params.dbVersion);
      } else {
        request = indexedDB.open(params.dbName, params.dbOptions);
      }
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
