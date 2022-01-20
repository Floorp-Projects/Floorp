/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from databaseShadowing-shared.js */
loadSubscript("databaseShadowing-shared.js");

async function testSteps() {
  // The shadow database was prepared in
  // test_databaseShadowing_clearOriginsByPrefix1.js

  disableNextGenLocalStorage();

  if (!importShadowDatabase("shadowdb-clearedOriginsByPrefix.sqlite")) {
    return;
  }

  verifyData([2, 3]);
}
