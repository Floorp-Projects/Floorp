"use strict";

let tmpFile = FileUtils.getDir("TmpD", []);
let dbConn;

add_task(async function setup() {
  tmpFile.append("TestDB");
  dbConn = await Sqlite.openConnection({ path: tmpFile.path });

  registerCleanupFunction(async () => {
    await dbConn.close();
    await IOUtils.remove(tmpFile.path);
  });
});

add_task(async function testgetRowsFromDBWithoutLocksRetries() {
  let deferred = Promise.withResolvers();
  let promise = MigrationUtils.getRowsFromDBWithoutLocks(
    tmpFile.path,
    "Temp DB",
    "SELECT * FROM moz_temp_table",
    deferred.promise
  );
  await new Promise(resolve => do_timeout(50, resolve));
  dbConn
    .execute("CREATE TABLE moz_temp_table (id INTEGER PRIMARY KEY)")
    .then(deferred.resolve);
  await promise;
});
