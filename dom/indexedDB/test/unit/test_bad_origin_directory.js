/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testGenerator */
var testGenerator = testSteps();

function* testSteps() {
  const url = "ws://ws.example.com";
  const name = "test_bad_origin_directory.js";

  let uri = Services.io.newURI(url);

  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  info("Opening database");

  let request = indexedDB.openForPrincipal(principal, name);
  request.onerror = continueToNextStepSync;
  request.onsuccess = unexpectedSuccessHandler;
  yield undefined;

  finishTest();
  yield undefined;
}
