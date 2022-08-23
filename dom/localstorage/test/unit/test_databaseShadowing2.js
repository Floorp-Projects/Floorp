/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from databaseShadowing-shared.js */
loadSubscript("databaseShadowing-shared.js");

add_task(async function testSteps() {
  // The shadow database was prepared in test_databaseShadowing1.js

  disableNextGenLocalStorage();

  ok(importShadowDatabase("shadowdb.sqlite"), "Import succeeded");

  verifyData([], /* migrated */ true);
});
