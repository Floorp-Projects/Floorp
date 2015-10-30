/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the various ways opening a cookie database can fail in a synchronous
// (i.e. immediate) manner, and that the database is renamed and recreated
// under each circumstance. These circumstances are, in no particular order:
//
// 1) A corrupt database, such that opening the connection fails.
// 2) The 'moz_cookies' table doesn't exist.
// 3) Not all of the expected columns exist, and statement creation fails when:
//    a) The schema version is larger than the current version.
//    b) The schema version is less than or equal to the current version.
// 4) Migration fails. This will have different modes depending on the initial
//    version:
//    a) Schema 1: the 'lastAccessed' column already exists.
//    b) Schema 2: the 'baseDomain' column already exists; or 'baseDomain'
//       cannot be computed for a particular host.
//    c) Schema 3: the 'creationTime' column already exists; or the
//       'moz_uniqueid' index already exists.

var COOKIE_DATABASE_SCHEMA_CURRENT = 7;

var test_generator = do_run_test();

function run_test() {
  do_test_pending();
  do_run_generator(test_generator);
}

function finish_test() {
  do_execute_soon(function() {
    test_generator.close();
    do_test_finished();
  });
}

function do_run_test() {
  // Set up a profile.
  this.profile = do_get_profile();

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  // Get the cookie file and the backup file.
  this.cookieFile = profile.clone();
  cookieFile.append("cookies.sqlite");
  this.backupFile = profile.clone();
  backupFile.append("cookies.sqlite.bak");
  do_check_false(cookieFile.exists());
  do_check_false(backupFile.exists());

  // Create a cookie object for testing.
  this.now = Date.now() * 1000;
  this.futureExpiry = Math.round(this.now / 1e6 + 1000);
  this.cookie = new Cookie("oh", "hai", "bar.com", "/", this.futureExpiry,
    this.now, this.now, false, false, false);

  this.sub_generator = run_test_1(test_generator);
  sub_generator.next();
  yield;

  this.sub_generator = run_test_2(test_generator);
  sub_generator.next();
  yield;

  this.sub_generator = run_test_3(test_generator, 99);
  sub_generator.next();
  yield;

  this.sub_generator = run_test_3(test_generator, COOKIE_DATABASE_SCHEMA_CURRENT);
  sub_generator.next();
  yield;

  this.sub_generator = run_test_3(test_generator, 4);
  sub_generator.next();
  yield;

  this.sub_generator = run_test_3(test_generator, 3);
  sub_generator.next();
  yield;

  this.sub_generator = run_test_4_exists(test_generator, 1,
    "ALTER TABLE moz_cookies ADD lastAccessed INTEGER");
  sub_generator.next();
  yield;

  this.sub_generator = run_test_4_exists(test_generator, 2,
    "ALTER TABLE moz_cookies ADD baseDomain TEXT");
  sub_generator.next();
  yield;

  this.sub_generator = run_test_4_baseDomain(test_generator);
  sub_generator.next();
  yield;

  this.sub_generator = run_test_4_exists(test_generator, 3,
    "ALTER TABLE moz_cookies ADD creationTime INTEGER");
  sub_generator.next();
  yield;

  this.sub_generator = run_test_4_exists(test_generator, 3,
    "CREATE UNIQUE INDEX moz_uniqueid ON moz_cookies (name, host, path)");
  sub_generator.next();
  yield;

  finish_test();
  return;
}

const garbage = "hello thar!";

function create_garbage_file(file)
{
  // Create an empty database file.
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, -1);
  do_check_true(file.exists());
  do_check_eq(file.fileSize, 0);

  // Write some garbage to it.
  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, -1, -1, 0);
  ostream.write(garbage, garbage.length);
  ostream.flush();
  ostream.close();

  file = file.clone(); // Windows maintains a stat cache. It's lame.
  do_check_eq(file.fileSize, garbage.length);
}

function check_garbage_file(file)
{
  do_check_true(file.exists());
  do_check_eq(file.fileSize, garbage.length);
  file.remove(false);
  do_check_false(file.exists());
}

function run_test_1(generator)
{
  // Create a garbage database file.
  create_garbage_file(cookieFile);

  // Load the profile and populate it.
  let uri = NetUtil.newURI("http://foo.com/");
  Services.cookies.setCookieString(uri, null, "oh=hai; max-age=1000", null);

  // Fake a profile change.
  do_close_profile(sub_generator);
  yield;
  do_load_profile();

  // Check that the new database contains the cookie, and the old file was
  // renamed.
  do_check_eq(do_count_cookies(), 1);
  check_garbage_file(backupFile);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Clean up.
  cookieFile.remove(false);
  do_check_false(cookieFile.exists());
  do_run_generator(generator);
}

function run_test_2(generator)
{
  // Load the profile and populate it.
  do_load_profile();
  let uri = NetUtil.newURI("http://foo.com/");
  Services.cookies.setCookieString(uri, null, "oh=hai; max-age=1000", null);

  // Fake a profile change.
  do_close_profile(sub_generator);
  yield;

  // Drop the table.
  let db = Services.storage.openDatabase(cookieFile);
  db.executeSimpleSQL("DROP TABLE moz_cookies");
  db.close();

  // Load the profile and check that the table is recreated in-place.
  do_load_profile();
  do_check_eq(do_count_cookies(), 0);
  do_check_false(backupFile.exists());

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Clean up.
  cookieFile.remove(false);
  do_check_false(cookieFile.exists());
  do_run_generator(generator);
}

function run_test_3(generator, schema)
{
  // Manually create a schema 2 database, populate it, and set the schema
  // version to the desired number.
  let schema2db = new CookieDatabaseConnection(do_get_cookie_file(profile), 2);
  schema2db.insertCookie(cookie);
  schema2db.db.schemaVersion = schema;
  schema2db.close();

  // Load the profile and check that the column existence test fails.
  do_load_profile();
  do_check_eq(do_count_cookies(), 0);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Check that the schema version has been reset.
  let db = Services.storage.openDatabase(cookieFile);
  do_check_eq(db.schemaVersion, COOKIE_DATABASE_SCHEMA_CURRENT);
  db.close();

  // Clean up.
  cookieFile.remove(false);
  do_check_false(cookieFile.exists());
  do_run_generator(generator);
}

function run_test_4_exists(generator, schema, stmt)
{
  // Manually create a database, populate it, and add the desired column.
  let db = new CookieDatabaseConnection(do_get_cookie_file(profile), schema);
  db.insertCookie(cookie);
  db.db.executeSimpleSQL(stmt);
  db.close();

  // Load the profile and check that migration fails.
  do_load_profile();
  do_check_eq(do_count_cookies(), 0);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Check that the schema version has been reset and the backup file exists.
  db = Services.storage.openDatabase(cookieFile);
  do_check_eq(db.schemaVersion, COOKIE_DATABASE_SCHEMA_CURRENT);
  db.close();
  do_check_true(backupFile.exists());

  // Clean up.
  cookieFile.remove(false);
  backupFile.remove(false);
  do_check_false(cookieFile.exists());
  do_check_false(backupFile.exists());
  do_run_generator(generator);
}

function run_test_4_baseDomain(generator)
{
  // Manually create a database and populate it with a bad host.
  let db = new CookieDatabaseConnection(do_get_cookie_file(profile), 2);
  let badCookie = new Cookie("oh", "hai", ".", "/", this.futureExpiry, this.now,
    this.now, false, false, false);
  db.insertCookie(badCookie);
  db.close();

  // Load the profile and check that migration fails.
  do_load_profile();
  do_check_eq(do_count_cookies(), 0);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Check that the schema version has been reset and the backup file exists.
  db = Services.storage.openDatabase(cookieFile);
  do_check_eq(db.schemaVersion, COOKIE_DATABASE_SCHEMA_CURRENT);
  db.close();
  do_check_true(backupFile.exists());

  // Clean up.
  cookieFile.remove(false);
  backupFile.remove(false);
  do_check_false(cookieFile.exists());
  do_check_false(backupFile.exists());
  do_run_generator(generator);
}
