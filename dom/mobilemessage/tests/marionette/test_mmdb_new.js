/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

startTestBase(function testCaseMain() {
  log("Test init MobileMessageDB");

  let mmdb = newMobileMessageDB();
  let dbName = "test_mmdb_new";
  let dbVersion = 0;
  let check = function() {
    is(mmdb.dbName, dbName, "dbName");
    if (!dbVersion) {
      ok(mmdb.dbVersion, "dbVersion");
      dbVersion = mmdb.dbVersion;
    } else {
      is(mmdb.dbVersion, dbVersion, "dbVersion");
    }
  };

  return initMobileMessageDB(mmdb, dbName, dbVersion)
    .then(check)
    .then(closeMobileMessageDB.bind(null, mmdb))
    .then(check)
    .then(function() {
      log("Test re-init and close.");
      return initMobileMessageDB(mmdb, dbName, dbVersion);
    })
    .then(check)
    .then(closeMobileMessageDB.bind(null, mmdb))
    .then(check);
});
