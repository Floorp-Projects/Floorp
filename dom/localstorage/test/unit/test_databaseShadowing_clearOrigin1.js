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

  let principal = getPrincipal("http://origin.test", {});
  clearOrigin(principal, "default", continueToNextStepSync);
  yield undefined;

  verifyData([1]);

  // Wait for all database connections to close.
  reset(continueToNextStepSync);
  yield undefined;

  exportShadowDatabase("shadowdb_clearedOrigin.sqlite");

  // The shadow database is now prepared for
  // test_databaseShadowing_clearOrigin2.js

  finishTest();
}
