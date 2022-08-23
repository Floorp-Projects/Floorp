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

  let request = clearOriginsByPattern(JSON.stringify({ userContextId: 15 }));
  await requestFinished(request);

  verifyData([4, 5, 6]);

  // Wait for all database connections to close.
  request = reset();
  await requestFinished(request);

  exportShadowDatabase("shadowdb-clearedOriginsByPattern.sqlite");

  // The shadow database is now prepared for
  // test_databaseShadowing_clearOriginsByPattern2.js
});
