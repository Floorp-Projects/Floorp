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
  pm.removeAll();

  let db = Services.storage.openDatabase(GetPermissionsFile(profile));
  db.schemaVersion = 10;

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
      origin,
      type,
      permission,
      expireType,
      expireTime,
      modificationTime,
    };
  }

  insertOrigin(
    "https://foo.com",
    "storageAccessAPI^https://foo.com",
    2,
    0,
    0,
    0
  );
  insertOrigin(
    "http://foo.com",
    "storageAccessAPI^https://bar.com^https://foo.com",
    2,
    0,
    0,
    0
  );
  insertOrigin(
    "http://foo.com",
    "storageAccessAPI^https://bar.com^https://baz.com",
    2,
    0,
    0,
    0
  );
  insertOrigin("http://foo.com^inBrowser=1", "A", 2, 0, 0, 0);

  // CLose the db connection
  stmt6Insert.finalize();
  db.close();
  db = null;

  let expected = [
    ["https://foo.com", "storageAccessAPI^https://foo.com", 2, 0, 0, 0],
    ["http://foo.com", "storageAccessAPI^https://bar.com", 2, 0, 0, 0],
    ["http://foo.com", "storageAccessAPI^https://bar.com", 2, 0, 0, 0],
    ["http://foo.com^inBrowser=1", "A", 2, 0, 0, 0],
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
    Assert.ok(!db.tableExists("moz_perms_v6"));

    let mozHostsCount = db.createStatement("SELECT count(*) FROM moz_hosts");
    try {
      mozHostsCount.executeStep();
      Assert.equal(mozHostsCount.getInt64(0), 0);
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
