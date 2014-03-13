/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'mmdb_head.js';

const DBNAME = "test_mmdb_new:" + newUUID();
let dbVersion = 0;

function check(aMmdb) {
  is(aMmdb.dbName, DBNAME, "dbName");
  if (!dbVersion) {
    ok(aMmdb.dbVersion, "dbVersion");
    dbVersion = aMmdb.dbVersion;
  } else {
    is(aMmdb.dbVersion, dbVersion, "dbVersion");
  }
}

startTestBase(function testCaseMain() {
  log("Test init MobileMessageDB");

  let mmdb = newMobileMessageDB();
  return initMobileMessageDB(mmdb, DBNAME, dbVersion)
    .then(() => check(mmdb))
    .then(() => closeMobileMessageDB(mmdb))
    .then(() => check(mmdb))

    .then(function() {
      log("Test re-init and close.");
      return initMobileMessageDB(mmdb, DBNAME, dbVersion);
    })
    .then(() => check(mmdb))
    .then(() => closeMobileMessageDB(mmdb))
    .then(() => check(mmdb));
});
