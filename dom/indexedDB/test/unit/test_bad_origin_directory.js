/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const url = "ftp://ftp.example.com";
  const name = "test_bad_origin_directory.js";

  let ios = SpecialPowers.Cc["@mozilla.org/network/io-service;1"]
                         .getService(SpecialPowers.Ci.nsIIOService);

  let uri = ios.newURI(url);

  let ssm = SpecialPowers.Cc["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(SpecialPowers.Ci.nsIScriptSecurityManager);

  let principal = ssm.createCodebasePrincipal(uri, {});

  info("Opening database");

  let request = indexedDB.openForPrincipal(principal, name);
  request.onerror = continueToNextStepSync;
  request.onsuccess = unexpectedSuccessHandler;
  yield undefined;

  finishTest();
  yield undefined;
}
