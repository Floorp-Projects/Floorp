/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
                               "resource://testing-common/PlacesTestUtils.jsm");

var PERMISSIONS_FILE_NAME = "permissions.sqlite";

function GetPermissionsFile(profile)
{
  let file = profile.clone();
  file.append(PERMISSIONS_FILE_NAME);
  return file;
}

add_task(async function test() {
  /* Create and set up the permissions database */
  let profile = do_get_profile();
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");

  let db = Services.storage.openDatabase(GetPermissionsFile(profile));
  db.schemaVersion = 9;
  db.executeSimpleSQL("DROP TABLE moz_perms");
  db.executeSimpleSQL("DROP TABLE IF EXISTS moz_hosts");

  db.executeSimpleSQL(
    "CREATE TABLE moz_perms (" +
      " id INTEGER PRIMARY KEY" +
      ",origin TEXT" +
      ",type TEXT" +
      ",permission INTEGER" +
      ",expireType INTEGER" +
      ",expireTime INTEGER" +
      ",modificationTime INTEGER" +
    ")");

  let stmt6Insert = db.createStatement(
    "INSERT INTO moz_perms (" +
      "id, origin, type, permission, expireType, expireTime, modificationTime" +
    ") VALUES (" +
      ":id, :origin, :type, :permission, :expireType, :expireTime, :modificationTime" +
    ")");

  db.executeSimpleSQL(
    "CREATE TABLE moz_hosts (" +
      " id INTEGER PRIMARY KEY" +
      ",host TEXT" +
      ",type TEXT" +
      ",permission INTEGER" +
      ",expireType INTEGER" +
      ",expireTime INTEGER" +
      ",modificationTime INTEGER" +
      ",appId INTEGER" +
      ",isInBrowserElement INTEGER" +
    ")");

  let stmtInsert = db.createStatement(
    "INSERT INTO moz_hosts (" +
      "id, host, type, permission, expireType, expireTime, modificationTime, appId, isInBrowserElement" +
    ") VALUES (" +
      ":id, :host, :type, :permission, :expireType, :expireTime, :modificationTime, :appId, :isInBrowserElement" +
    ")");

  let id = 0;

  function insertOrigin(origin, type, permission, expireType, expireTime, modificationTime) {
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
      origin,
      type,
      permission,
      expireType,
      expireTime,
      modificationTime
    };
  }

  function insertHost(host, type, permission, expireType, expireTime, modificationTime, appId, isInBrowserElement) {
    let thisId = id++;

    stmtInsert.bindByName("id", thisId);
    stmtInsert.bindByName("host", host);
    stmtInsert.bindByName("type", type);
    stmtInsert.bindByName("permission", permission);
    stmtInsert.bindByName("expireType", expireType);
    stmtInsert.bindByName("expireTime", expireTime);
    stmtInsert.bindByName("modificationTime", modificationTime);
    stmtInsert.bindByName("appId", appId);
    stmtInsert.bindByName("isInBrowserElement", isInBrowserElement);

    try {
      stmtInsert.execute();
    } finally {
      stmtInsert.reset();
    }

    return {
      id: thisId,
      host,
      type,
      permission,
      expireType,
      expireTime,
      modificationTime,
      appId,
      isInBrowserElement
    };
  }

  let created7 = [
    insertOrigin("https://foo.com", "A", 2, 0, 0, 0),
    insertOrigin("http://foo.com", "A", 2, 0, 0, 0),
    insertOrigin("http://foo.com^inBrowser=1", "A", 2, 0, 0, 0),
  ];

  // Add some rows to the database
  let created = [
    insertHost("foo.com", "A", 1, 0, 0, 0, 0, false),
    insertHost("foo.com", "B", 1, 0, 0, 0, 1000, false),
    insertHost("foo.com", "C", 1, 0, 0, 0, 2000, true),
  ];

  // CLose the db connection
  stmt6Insert.finalize();
  stmtInsert.finalize();
  db.close();
  stmtInsert = null;
  db = null;

  let expected = [
    ["https://foo.com", "A", 2, 0, 0, 0],
    ["http://foo.com", "A", 2, 0, 0, 0],
    ["http://foo.com^inBrowser=1", "A", 2, 0, 0, 0],
  ];

  let found = expected.map((it) => 0);

  // Add some places to the places database
  await PlacesTestUtils.addVisits(Services.io.newURI("https://foo.com/some/other/subdirectory"));
  await PlacesTestUtils.addVisits(Services.io.newURI("ftp://some.subdomain.of.foo.com:8000/some/subdirectory"));
  await PlacesTestUtils.addVisits(Services.io.newURI("ftp://127.0.0.1:8080"));
  await PlacesTestUtils.addVisits(Services.io.newURI("https://localhost:8080"));

  // This will force the permission-manager to reload the data.
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");

  // Force initialization of the nsPermissionManager
  for (let permission of Services.perms.enumerator) {
    let isExpected = false;

    expected.forEach((it, i) => {
      if (permission.principal.origin == it[0] &&
          permission.type == it[1] &&
          permission.capability == it[2] &&
          permission.expireType == it[3] &&
          permission.expireTime == it[4]) {
        isExpected = true;
        found[i]++;
      }
    });

    Assert.ok(isExpected,
              "Permission " + (isExpected ? "should" : "shouldn't") +
              " be in permission database: " +
              permission.principal.origin + ", " +
              permission.type + ", " +
              permission.capability + ", " +
              permission.expireType + ", " +
              permission.expireTime);
  }

  found.forEach((count, i) => {
    Assert.ok(count == 1, "Expected count = 1, got count = " + count + " for permission " + expected[i]);
  });

  // Check to make sure that all of the tables which we care about are present
  {
    db = Services.storage.openDatabase(GetPermissionsFile(profile));
    Assert.ok(db.tableExists("moz_perms"));
    Assert.ok(db.tableExists("moz_hosts"));
    Assert.ok(!db.tableExists("moz_perms_v6"));

    let mozHostsCount = db.createStatement("SELECT count(*) FROM moz_hosts");
    try {
      mozHostsCount.executeStep();
      Assert.equal(mozHostsCount.getInt64(0), 3);
    } finally {
      mozHostsCount.finalize();
    }

    let mozPermsCount = db.createStatement("SELECT count(*) FROM moz_perms");
    try {
      mozPermsCount.executeStep();
      Assert.equal(mozPermsCount.getInt64(0), expected.length);
    } finally {
      mozPermsCount.finalize();
    }

    db.close();
  }
});
