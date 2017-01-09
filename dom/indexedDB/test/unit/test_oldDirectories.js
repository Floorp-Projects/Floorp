/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  // This lives in storage/default/http+++www.mozilla.org
  const url = "http://www.mozilla.org";
  const dbName = "dbC";
  const dbVersion = 1;

  let ios = SpecialPowers.Cc["@mozilla.org/network/io-service;1"]
                         .getService(SpecialPowers.Ci.nsIIOService);

  let ssm = SpecialPowers.Cc["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(SpecialPowers.Ci.nsIScriptSecurityManager);

  function openDatabase() {
    let uri = ios.newURI(url);
    let principal = ssm.createCodebasePrincipal(uri, {});
    let request = indexedDB.openForPrincipal(principal, dbName, dbVersion);
    return request;
  }

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  installPackagedProfile("oldDirectories_profile");

  let request = openDatabase();
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  is(event.type, "success", "Correct event type");

  resetAllDatabases(continueToNextStepSync);
  yield undefined;

  request = openDatabase();
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  is(event.type, "success", "Correct event type");

  let directoryService = Cc["@mozilla.org/file/directory_service;1"]
                         .getService(Ci.nsIProperties);

  let profileDir = directoryService.get("ProfD", Ci.nsIFile);

  let dir = profileDir.clone();
  dir.append("indexedDB");

  let exists = dir.exists();
  ok(!exists, "indexedDB doesn't exist");

  dir = profileDir.clone();
  dir.append("storage");
  dir.append("persistent");

  exists = dir.exists();
  ok(!exists, "storage/persistent doesn't exist");

  finishTest();
  yield undefined;
}
