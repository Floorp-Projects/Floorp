/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let PERMISSIONS_FILE_NAME = "permissions.sqlite";

function GetPermissionsFile(profile)
{
  let file = profile.clone();
  file.append(PERMISSIONS_FILE_NAME);
  return file;
}

function run_test() {
  run_next_test();
}

add_task(function test() {
  /* Create and set up the permissions database */
  let profile = do_get_profile();
  let perms = [];

  let db = Services.storage.openDatabase(GetPermissionsFile(profile));
  db.schemaVersion = 5;

  db.executeSimpleSQL(
    "CREATE TABLE moz_hosts (" +
      " id INTEGER PRIMARY KEY" +
      ",origin TEXT" +
      ",type TEXT" +
      ",permission INTEGER" +
      ",expireType INTEGER" +
      ",expireTime INTEGER" +
      ",modificationTime INTEGER" +
    ")");

  db.executeSimpleSQL(
    "CREATE TABLE moz_hosts_v4 (" +
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
      "id, origin, type, permission, expireType, expireTime, modificationTime" +
    ") VALUES (" +
      ":id, :origin, :type, :permission, :expireType, :expireTime, :modificationTime" +
    ")");

  let stmt4Insert = db.createStatement(
    "INSERT INTO moz_hosts_v4 (" +
      "id, host, type, permission, expireType, expireTime, modificationTime, appId, isInBrowserElement" +
    ") VALUES (" +
      ":id, :host, :type, :permission, :expireType, :expireTime, :modificationTime, :appId, :isInBrowserElement" +
    ")");

  let id = 0;

  // Insert an origin into the database, and return its principal, type, and permission values
  function insertV5(origin, type, permission, expireType, expireTime, modificationTime) {
    let thisId = id++;

    stmtInsert.bindByName("id", thisId);
    stmtInsert.bindByName("origin", origin);
    stmtInsert.bindByName("type", type);
    stmtInsert.bindByName("permission", permission);
    stmtInsert.bindByName("expireType", expireType);
    stmtInsert.bindByName("expireTime", expireTime);
    stmtInsert.bindByName("modificationTime", modificationTime);

    stmtInsert.execute();

    return function(stmtLookup, stmt4Lookup) {
      stmtLookup.bindByName("id", thisId);

      do_check_true(stmtLookup.executeStep());
      do_check_eq(stmtLookup.getUTF8String(0), origin);
      do_check_eq(stmtLookup.getUTF8String(1), type);
      do_check_eq(stmtLookup.getInt64(2), permission);
      do_check_eq(stmtLookup.getInt64(3), expireType);
      do_check_eq(stmtLookup.getInt64(4), expireTime);
      do_check_eq(stmtLookup.getInt64(5), modificationTime);
      do_check_false(stmtLookup.executeStep());
      stmtLookup.reset();
    };
  }

  function insertV4(host, type, permission, expireType, expireTime, modificationTime, appId, isInBrowserElement) {
    let thisId = id++;

    stmt4Insert.bindByName("id", thisId);
    stmt4Insert.bindByName("host", host);
    stmt4Insert.bindByName("type", type);
    stmt4Insert.bindByName("permission", permission);
    stmt4Insert.bindByName("expireType", expireType);
    stmt4Insert.bindByName("expireTime", expireTime);
    stmt4Insert.bindByName("modificationTime", modificationTime);
    stmt4Insert.bindByName("appId", appId);
    stmt4Insert.bindByName("isInBrowserElement", isInBrowserElement);

    stmt4Insert.execute();

    return function(stmtLookup, stmt4Lookup) {
      stmt4Lookup.bindByName("id", thisId);

      do_check_true(stmt4Lookup.executeStep());
      do_check_eq(stmt4Lookup.getUTF8String(0), host);
      do_check_eq(stmt4Lookup.getUTF8String(1), type);
      do_check_eq(stmt4Lookup.getInt64(2), permission);
      do_check_eq(stmt4Lookup.getInt64(3), expireType);
      do_check_eq(stmt4Lookup.getInt64(4), expireTime);
      do_check_eq(stmt4Lookup.getInt64(5), modificationTime);
      do_check_eq(stmt4Lookup.getInt64(6), appId);
      do_check_eq(!!stmt4Lookup.getInt64(7), isInBrowserElement);
      do_check_false(stmt4Lookup.executeStep());
      stmt4Lookup.reset();
    };
  }

  // Add some rows to the database
  perms = [
    insertV5("http://foo.com", "A", 1, 0, 0, 0),
    insertV5("https://foo.com", "A", 1, 0, 0, 0),
    insertV5("https://foo.com^appId=1000", "A", 1, 0, 0, 0),
    insertV5("http://foo.com^appId=2000&inBrowser=1", "A", 1, 0, 0, 0),
    insertV5("https://sub.foo.com", "B", 1, 0, 0, 0),
    insertV5("http://subber.sub.foo.com", "B", 1, 0, 0, 0),
    insertV5("http://bar.ca", "B", 1, 0, 0, 0),
    insertV5("https://bar.ca", "B", 1, 0, 0, 0),
    insertV5("ftp://bar.ca", "A", 2, 0, 0, 0),
    insertV5("http://bar.ca^appId=1000", "B", 1, 0, 0, 0),
    insertV5("http://bar.ca^appId=1000&inBrowser=1", "A", 1, 0, 0, 0),
    insertV5("file:///some/path/to/file.html", "A", 1, 0, 0, 0),
    insertV5("file:///another/file.html", "A", 1, 0, 0, 0),
    insertV5("about:home", "A", 1, 0, 0, 0),


    insertV4("https://foo.com", "A", 1, 0, 0, 0, 1000, false),
    insertV4("http://foo.com", "A", 1, 0, 0, 0, 2000, true),
    insertV4("https://sub.foo.com", "B", 1, 0, 0, 0, 0, false),
    insertV4("http://subber.sub.foo.com", "B", 1, 0, 0, 0, 0, false),
    insertV4("http://bar.ca", "B", 1, 0, 0, 0, 0, false),
    insertV4("https://bar.ca", "B", 1, 0, 0, 0, 0, false),
  ];

  // Force the permission manager to initialize
  let enumerator = Services.perms.enumerator;
  do_check_true(enumerator.hasMoreElements());

  stmtInsert.finalize();
  stmt4Insert.finalize();
  db.close();

  db = Services.storage.openDatabase(GetPermissionsFile(profile));
  do_check_eq(db.schemaVersion, 6);

  let stmtLookup = db.createStatement(
    "SELECT origin, type, permission, expireType, expireTime, modificationTime FROM moz_perms WHERE id = :id;");
  let stmt4Lookup = db.createStatement(
    "SELECT host, type, permission, expireType, expireTime, modificationTime, appId, isInBrowserElement FROM moz_hosts WHERE id = :id;");

  // Call each of the validation callbacks
  perms.forEach((cb) => cb(stmtLookup, stmt4Lookup));

  // Close the db connection
  stmtLookup.finalize();
  stmt4Lookup.finalize();
  db.close();
});
