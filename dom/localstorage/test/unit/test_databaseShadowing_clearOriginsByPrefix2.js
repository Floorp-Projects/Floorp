/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from databaseShadowing-shared.js */
loadSubscript("databaseShadowing-shared.js");

add_task(async function testSteps() {
  // The shadow database was prepared in
  // test_databaseShadowing_clearOriginsByPrefix1.js

  disableNextGenLocalStorage();

  ok(
    importShadowDatabase("shadowdb-clearedOriginsByPrefix.sqlite"),
    "Import succeeded"
  );

  verifyData([2, 3], /* migrated */ true);
});
