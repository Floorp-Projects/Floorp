/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const openParams = [
    // This one lives in storage/permanent/chrome
    { dbName: "dbA",
      dbOptions: { version: 1, storage: "persistent" } },

    // This one lives in storage/temporary/http+++localhost
    { url: "http://localhost", dbName: "dbB",
      dbOptions: { version: 1, storage: "temporary" } },

    // This one lives in storage/default/http+++localhost+81
    { url: "http://localhost:81", dbName: "dbC",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+82
    { url: "http://localhost:82", dbName: "dbD",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+83
    { url: "http://localhost:83", dbName: "dbE",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+84
    { url: "http://localhost:84", dbName: "dbF",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+85
    { url: "http://localhost:85", dbName: "dbG",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+86
    { url: "http://localhost:86", dbName: "dbH",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+87
    { url: "http://localhost:87", dbName: "dbI",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+88
    { url: "http://localhost:88", dbName: "dbJ",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+89
    { url: "http://localhost:89", dbName: "dbK",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+90
    { url: "http://localhost:90", dbName: "dbL",
      dbOptions: { version: 1, storage: "default" } }
  ];

  let ios = SpecialPowers.Cc["@mozilla.org/network/io-service;1"]
                         .getService(SpecialPowers.Ci.nsIIOService);

  let ssm = SpecialPowers.Cc["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(SpecialPowers.Ci.nsIScriptSecurityManager);

  function openDatabase(params) {
    let request;
    if ("url" in params) {
      let uri = ios.newURI(params.url);
      let principal = ssm.createCodebasePrincipal(uri, {});
      request = indexedDB.openForPrincipal(principal, params.dbName,
                                           params.dbOptions);
    } else {
      request = indexedDB.open(params.dbName, params.dbOptions);
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
