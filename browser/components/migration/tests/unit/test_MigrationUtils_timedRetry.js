"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

let tmpFile = FileUtils.getDir("TmpD", [], true);
let dbConn;

add_task(async function setup() {
  tmpFile.append("TestDB");
  dbConn = await Sqlite.openConnection({ path: tmpFile.path });

  registerCleanupFunction(() => {
    dbConn.close();
    OS.File.remove(tmpFile.path);
  });
});

add_task(async function testgetRowsFromDBWithoutLocksRetries() {
  let promise = MigrationUtils.getRowsFromDBWithoutLocks(tmpFile.path,
                                                         "Temp DB",
                                                         "SELECT * FROM moz_temp_table");
  await new Promise(resolve => do_timeout(50, resolve));
  dbConn.execute("CREATE TABLE moz_temp_table (id INTEGER PRIMARY KEY)");
  await promise;
});
