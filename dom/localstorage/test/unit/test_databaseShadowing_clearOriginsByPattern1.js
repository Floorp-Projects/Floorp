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

  clearOriginsByPattern(JSON.stringify({ userContextId: 15 }),
                        continueToNextStepSync);
  yield undefined;

  verifyData([4,5,6]);

  // Wait for all database connections to close.
  reset(continueToNextStepSync);
  yield undefined;

  exportShadowDatabase("shadowdb-clearedOriginsByPattern.sqlite");

  // The shadow database is now prepared for
  // test_databaseShadowing_clearOriginsByPattern2.js

  finishTest();
}
