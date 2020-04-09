/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);

var PERMISSIONS_FILE_NAME = "permissions.sqlite";

function GetPermissionsFile(profile) {
  let file = profile.clone();
  file.append(PERMISSIONS_FILE_NAME);
  return file;
}

add_task(function test() {
  /* Create and set up the permissions database */
  let profile = do_get_profile();
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");

  // We need to execute a pm method to be sure that the DB is fully
  // initialized.
  var pm = Services.perms;
  pm.removeAll();

  let db = Services.storage.openDatabase(GetPermissionsFile(profile));
  db.schemaVersion = 6;
  db.executeSimpleSQL("DROP TABLE moz_perms");
  db.executeSimpleSQL("DROP TABLE moz_hosts");

  /*
   * V5 table
   */
  db.executeSimpleSQL(
    "CREATE TABLE moz_perms (" +
      " id INTEGER PRIMARY KEY" +
      ",origin TEXT" +
      ",type TEXT" +
      ",permission INTEGER" +
      ",expireType INTEGER" +
      ",expireTime INTEGER" +
      ",modificationTime INTEGER" +
      ")"
  );

  let stmt6Insert = db.createStatement(
    "INSERT INTO moz_perms (" +
      "id, origin, type, permission, expireType, expireTime, modificationTime" +
      ") VALUES (" +
      ":id, :origin, :type, :permission, :expireType, :expireTime, :modificationTime" +
      ")"
  );

  let id = 0;
  function insertOrigin(
    origin,
    type,
    permission,
    expireType,
    expireTime,
    modificationTime
  ) {
    let thisId = id++;

    stmt6Insert.bindByName("id", thisId);
    stmt6Insert.bindByName("origin", origin);
    stmt6Insert.bindByName("type", type);
    stmt6Insert.bindByName("permission", permission);
    stmt6Insert.bindByName("expireType", expireType);
    stmt6Insert.bindByName("expireTime", expireTime);
    stmt6Insert.bindByName("modificationTime", modificationTime);

    try {
      stmt6Insert.execute();
    } finally {
      stmt6Insert.reset();
    }

    return {
      id: thisId,
      host: origin,
      type,
      permission,
      expireType,
      expireTime,
      modificationTime,
    };
  }

  // eslint-disable-next-line no-unused-vars
  let created6 = [
    insertOrigin("https://foo.com", "A", 2, 0, 0, 0),
    insertOrigin("http://foo.com", "A", 2, 0, 0, 0),
    insertOrigin("http://foo.com^appId=1000&inBrowser=1", "A", 2, 0, 0, 0),
  ];

  let created4 = []; // Didn't create any v4 entries, so the DB should be empty

  // CLose the db connection
  stmt6Insert.finalize();
  db.close();
  stmt6Insert = null;
  db = null;

  let expected = [
    ["https://foo.com", "A", 2, 0, 0, 0],
    ["http://foo.com", "A", 2, 0, 0, 0],
    ["http://foo.com^inBrowser=1", "A", 2, 0, 0, 0],
  ];

  let found = expected.map(it => 0);

  // This will force the permission-manager to reload the data.
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");

  // Force initialization of the PermissionManager
  for (let permission of Services.perms.all) {
    let isExpected = false;

    expected.forEach((it, i) => {
      if (
        permission.principal.origin == it[0] &&
        permission.type == it[1] &&
        permission.capability == it[2] &&
        permission.expireType == it[3] &&
        permission.expireTime == it[4]
      ) {
        isExpected = true;
        found[i]++;
      }
    });

    Assert.ok(
      isExpected,
      "Permission " +
        (isExpected ? "should" : "shouldn't") +
        " be in permission database: " +
        permission.principal.origin +
        ", " +
        permission.type +
        ", " +
        permission.capability +
        ", " +
        permission.expireType +
        ", " +
        permission.expireTime
    );
  }

  found.forEach((count, i) => {
    Assert.ok(
      count == 1,
      "Expected count = 1, got count = " +
        count +
        " for permission " +
        expected[i]
    );
  });

  // Check to make sure that all of the tables which we care about are present
  {
    db = Services.storage.openDatabase(GetPermissionsFile(profile));
    Assert.ok(db.tableExists("moz_perms"));
    Assert.ok(db.tableExists("moz_hosts"));
    Assert.ok(!db.tableExists("moz_hosts_is_backup"));
    Assert.ok(!db.tableExists("moz_perms_v6"));

    let mozHostsStmt = db.createStatement(
      "SELECT " +
        "host, type, permission, expireType, expireTime, " +
        "modificationTime, isInBrowserElement " +
        "FROM moz_hosts WHERE id = :id"
    );
    try {
      // Check that the moz_hosts table still contains the correct values.
      created4.forEach(it => {
        mozHostsStmt.reset();
        mozHostsStmt.bindByName("id", it.id);
        mozHostsStmt.executeStep();
        Assert.equal(mozHostsStmt.getUTF8String(0), it.host);
        Assert.equal(mozHostsStmt.getUTF8String(1), it.type);
        Assert.equal(mozHostsStmt.getInt64(2), it.permission);
        Assert.equal(mozHostsStmt.getInt64(3), it.expireType);
        Assert.equal(mozHostsStmt.getInt64(4), it.expireTime);
        Assert.equal(mozHostsStmt.getInt64(5), it.modificationTime);
        Assert.equal(mozHostsStmt.getInt64(6), it.isInBrowserElement);
      });
    } finally {
      mozHostsStmt.finalize();
    }

    // Check that there are the right number of values
    let mozHostsCount = db.createStatement("SELECT count(*) FROM moz_hosts");
    try {
      mozHostsCount.executeStep();
      Assert.equal(mozHostsCount.getInt64(0), created4.length);
    } finally {
      mozHostsCount.finalize();
    }

    db.close();
  }
});
