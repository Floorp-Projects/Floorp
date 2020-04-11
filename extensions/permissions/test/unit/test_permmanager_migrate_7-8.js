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

add_task(async function test() {
  /* Create and set up the permissions database */
  let profile = do_get_profile();
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");

  // We need to execute a pm method to be sure that the DB is fully
  // initialized.
  var pm = Services.perms;
  Assert.equal(pm.all.length, 0, "No cookies");

  let db = Services.storage.openDatabase(GetPermissionsFile(profile));
  db.schemaVersion = 7;
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

  /*
   * V4 table
   */
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
      ")"
  );

  let stmtInsert = db.createStatement(
    "INSERT INTO moz_hosts (" +
      "id, host, type, permission, expireType, expireTime, modificationTime, appId, isInBrowserElement" +
      ") VALUES (" +
      ":id, :host, :type, :permission, :expireType, :expireTime, :modificationTime, :appId, :isInBrowserElement" +
      ")"
  );

  /*
   * The v4 table is a backup
   */
  db.executeSimpleSQL(
    "CREATE TABLE moz_hosts_is_backup (dummy INTEGER PRIMARY KEY)"
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
      origin,
      type,
      permission,
      expireType,
      expireTime,
      modificationTime,
    };
  }

  function insertHost(
    host,
    type,
    permission,
    expireType,
    expireTime,
    modificationTime,
    appId,
    isInBrowserElement
  ) {
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
      isInBrowserElement,
    };
  }
  // eslint-disable-next-line no-unused-vars
  let created7 = [
    insertOrigin("https://foo.com", "A", 2, 0, 0, 0),
    insertOrigin("http://foo.com", "A", 2, 0, 0, 0),
    insertOrigin("http://foo.com^inBrowser=1", "A", 2, 0, 0, 0),
    insertOrigin("https://192.0.2.235", "A", 2, 0, 0),
  ];

  // Add some rows to the database
  // eslint-disable-next-line no-unused-vars
  let created = [
    insertHost("foo.com", "A", 1, 0, 0, 0, 0, false),
    insertHost("foo.com", "C", 1, 0, 0, 0, 0, false),
    insertHost("foo.com", "A", 1, 0, 0, 0, 1000, false),
    insertHost("foo.com", "A", 1, 0, 0, 0, 2000, true),
    insertHost("sub.foo.com", "B", 1, 0, 0, 0, 0, false),
    insertHost("subber.sub.foo.com", "B", 1, 0, 0, 0, 0, false),
    insertHost("bar.ca", "B", 1, 0, 0, 0, 0, false),
    insertHost("bar.ca", "B", 1, 0, 0, 0, 1000, false),
    insertHost("bar.ca", "A", 1, 0, 0, 0, 1000, true),
    insertHost("localhost", "A", 1, 0, 0, 0, 0, false),
    insertHost("127.0.0.1", "A", 1, 0, 0, 0, 0, false),
    insertHost("192.0.2.235", "A", 1, 0, 0, 0, 0, false),
    // Although ipv6 addresses are written with [] around the IP address,
    // the .host property doesn't contain these []s, which means that we
    // write it like this
    insertHost("2001:db8::ff00:42:8329", "C", 1, 0, 0, 0, 0, false),
    insertHost("file:///some/path/to/file.html", "A", 1, 0, 0, 0, 0, false),
    insertHost("file:///another/file.html", "A", 1, 0, 0, 0, 0, false),
    insertHost(
      "moz-nullprincipal:{8695105a-adbe-4e4e-8083-851faa5ca2d7}",
      "A",
      1,
      0,
      0,
      0,
      0,
      false
    ),
    insertHost(
      "moz-nullprincipal:{12ahjksd-akjs-asd3-8393-asdu2189asdu}",
      "B",
      1,
      0,
      0,
      0,
      0,
      false
    ),
    insertHost("<file>", "A", 1, 0, 0, 0, 0, false),
    insertHost("<file>", "B", 1, 0, 0, 0, 0, false),
  ];

  // CLose the db connection
  stmt6Insert.finalize();
  stmtInsert.finalize();
  db.close();
  stmtInsert = null;
  db = null;

  let expected = [
    // We should have kept the previously migrated entries
    ["https://foo.com", "A", 2, 0, 0, 0],
    ["http://foo.com", "A", 2, 0, 0, 0],
    ["http://foo.com^inBrowser=1", "A", 2, 0, 0, 0],

    // Make sure that we also support localhost, and IP addresses
    ["https://localhost:8080", "A", 1, 0, 0],
    ["ftp://127.0.0.1:8080", "A", 1, 0, 0],

    ["http://[2001:db8::ff00:42:8329]", "C", 1, 0, 0],
    ["https://[2001:db8::ff00:42:8329]", "C", 1, 0, 0],
    ["http://192.0.2.235", "A", 1, 0, 0],

    // There should only be one entry of this type in the database
    ["https://192.0.2.235", "A", 2, 0, 0],
  ];

  let found = expected.map(it => 0);

  // Add some places to the places database
  await PlacesTestUtils.addVisits(
    Services.io.newURI("https://foo.com/some/other/subdirectory")
  );
  await PlacesTestUtils.addVisits(
    Services.io.newURI("ftp://some.subdomain.of.foo.com:8000/some/subdirectory")
  );
  await PlacesTestUtils.addVisits(Services.io.newURI("ftp://127.0.0.1:8080"));
  await PlacesTestUtils.addVisits(Services.io.newURI("https://localhost:8080"));

  // This will force the permission-manager to reload the data.
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");

  // Force initialization of the nsPermissionManager
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

    // The moz_hosts table should still exist but be empty
    let mozHostsCount = db.createStatement("SELECT count(*) FROM moz_hosts");
    try {
      mozHostsCount.executeStep();
      Assert.equal(mozHostsCount.getInt64(0), 0);
    } finally {
      mozHostsCount.finalize();
    }

    // Check that there are the right number of values in the permissions database
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
