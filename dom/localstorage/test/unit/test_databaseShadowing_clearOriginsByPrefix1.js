/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from databaseShadowing-shared.js */
loadSubscript("databaseShadowing-shared.js");

add_task(async function testSteps() {
  enableNextGenLocalStorage();

  storeData();

  verifyData([]);

  let principal = getPrincipal("http://prefix.test", {});
  let request = clearOriginsByPrefix(principal, "default");
  await requestFinished(request);

  // Wait for all database connections to close.
  request = reset();
  await requestFinished(request);

  exportShadowDatabase("shadowdb-clearedOriginsByPrefix.sqlite");

  // The shadow database is now prepared for
  // test_databaseShadowing_clearOriginsByPrefix2.js
});
