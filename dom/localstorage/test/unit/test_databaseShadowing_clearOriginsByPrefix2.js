/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

loadSubscript("databaseShadowing-shared.js");

function* testSteps()
{
  // The shadow database was prepared in
  // test_databaseShadowing_clearOriginsByPrefix1.js

  disableNextGenLocalStorage();

  if (!importShadowDatabase("shadowdb-clearedOriginsByPrefix.sqlite")) {
    finishTest();
    return;
  }

  verifyData([2,3]);

  finishTest();
}
