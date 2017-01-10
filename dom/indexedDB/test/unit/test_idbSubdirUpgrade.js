/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const openParams = [
    // This one lives in storage/default/http+++www.mozilla.org
    { url: "http://www.mozilla.org", dbName: "dbB", dbVersion: 1 },

    // This one lives in storage/default/1007+t+https+++developer.cdn.mozilla.net
    { appId: 1007, inIsolatedMozBrowser: true, url: "https://developer.cdn.mozilla.net",
      dbName: "dbN", dbVersion: 1 },
  ];

  let ios = SpecialPowers.Cc["@mozilla.org/network/io-service;1"]
                         .getService(SpecialPowers.Ci.nsIIOService);

  let ssm = SpecialPowers.Cc["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(SpecialPowers.Ci.nsIScriptSecurityManager);

  function openDatabase(params) {
    let uri = ios.newURI(params.url);
    let principal =
      ssm.createCodebasePrincipal(uri,
                                  {appId: params.appId || ssm.NO_APPID,
                                   inIsolatedMozBrowser: params.inIsolatedMozBrowser});
    let request = indexedDB.openForPrincipal(principal, params.dbName,
                                             params.dbVersion);
    return request;
  }

  for (let i = 1; i <= 2; i++) {
    clearAllDatabases(continueToNextStepSync);
    yield undefined;

    installPackagedProfile("idbSubdirUpgrade" + i + "_profile");

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
  }

  finishTest();
  yield undefined;
}
