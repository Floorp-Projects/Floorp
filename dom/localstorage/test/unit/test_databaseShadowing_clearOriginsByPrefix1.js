/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

loadSubscript("databaseShadowing-shared.js");

function* testSteps()
{
  enableNextGenLocalStorage();

  storeData();

  verifyData([]);

  let principal = getPrincipal("http://prefix.test", {});
  clearOriginsByPrefix(principal, "default", continueToNextStepSync);
  yield undefined;

  // Wait for all database connections to close.
  reset(continueToNextStepSync);
  yield undefined;

  exportShadowDatabase("shadowdb-clearedOriginsByPrefix.sqlite");

  // The shadow database is now prepared for
  // test_databaseShadowing_clearOriginsByPrefix2.js

  finishTest();
}
