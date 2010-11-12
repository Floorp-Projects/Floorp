/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the various ways opening a cookie database can fail in an asynchronous
// (i.e. after synchronous initialization) manner, and that the database is
// renamed and recreated under each circumstance. These circumstances are, in no
// particular order:
//
// 1) A write operation failing after the database has been read in.
// 2) Asynchronous read failure due to a corrupt database.
// 3) Synchronous read failure due to a corrupt database, when reading:
//    a) a single base domain;
//    b) the entire database.
// 4) Asynchronous read failure, followed by another failure during INSERT but
//    before the database closes for rebuilding. (The additional error should be
//    ignored.)
// 5) Asynchronous read failure, followed by an INSERT failure during rebuild.
//    This should result in an abort of the database rebuild; the partially-
//    built database should be moved to 'cookies.sqlite.bak-rebuild'.

let test_generator = do_run_test();

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

  // Get the cookie file and the backup file.
  do_check_false(do_get_cookie_file(profile).exists());
  do_check_false(do_get_backup_file(profile).exists());

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

  this.sub_generator = run_test_3(test_generator);
  sub_generator.next();
  yield;

  this.sub_generator = run_test_4(test_generator);
  sub_generator.next();
  yield;

  this.sub_generator = run_test_5(test_generator);
  sub_generator.next();
  yield;

  finish_test();
  return;
}

function do_get_backup_file(profile)
{
  let file = profile.clone();
  file.append("cookies.sqlite.bak");
  return file;
}

function do_get_rebuild_backup_file(profile)
{
  let file = profile.clone();
  file.append("cookies.sqlite.bak-rebuild");
  return file;
}

function do_corrupt_db(file)
{
  // Sanity check: the database size should be larger than 450k, since we've
  // written about 460k of data. If it's not, let's make it obvious now.
  let size = file.fileSize;
  do_check_true(size > 450e3);

  // Corrupt the database by writing bad data to the end of the file. We
  // assume that the important metadata -- table structure etc -- is stored
  // elsewhere, and that doing this will not cause synchronous failure when
  // initializing the database connection. This is totally empirical --
  // overwriting between 1k and 100k of live data seems to work. (Note that the
  // database file will be larger than the actual content requires, since the
  // cookie service uses a large growth increment. So we calculate the offset
  // based on the expected size of the content, not just the file size.)
  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, 2, -1, 0);
  let sstream = ostream.QueryInterface(Ci.nsISeekableStream);
  let n = size - 450e3 + 20e3;
  sstream.seek(Ci.nsISeekableStream.NS_SEEK_SET, size - n);
  for (let i = 0; i < n; ++i) {
    ostream.write("a", 1);
  }
  ostream.flush();
  ostream.close();

  do_check_eq(file.clone().fileSize, size);
  return size;
}

function run_test_1(generator)
{
  // Load the profile and populate it.
  let uri = NetUtil.newURI("http://foo.com/");
  Services.cookies.setCookieString(uri, null, "oh=hai; max-age=1000", null);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Open a database connection now, before we load the profile and begin
  // asynchronous write operations. In order to tell when the async delete
  // statement has completed, we do something tricky: open a schema 2 connection
  // and add a cookie with null baseDomain. We can then wait until we see it
  // deleted in the new database.
  let db2 = new CookieDatabaseConnection(do_get_cookie_file(profile), 2);
  db2.db.executeSimpleSQL("INSERT INTO moz_cookies (baseDomain) VALUES (NULL)");
  db2.close();
  let db = new CookieDatabaseConnection(do_get_cookie_file(profile), 4);
  do_check_eq(do_count_cookies_in_db(db.db), 2);

  // Load the profile, and wait for async read completion...
  do_load_profile(sub_generator);
  yield;

  // ... and the DELETE statement to finish.
  while (do_count_cookies_in_db(db.db) == 2) {
    do_execute_soon(function() {
      do_run_generator(sub_generator);
    });
    yield;
  }
  do_check_eq(do_count_cookies_in_db(db.db), 1);

  // Insert a row.
  db.insertCookie(cookie);
  db.close();

  // Attempt to insert a cookie with the same (name, host, path) triplet.
  Services.cookiemgr.add(cookie.host, cookie.path, cookie.name, "hallo",
    cookie.isSecure, cookie.isHttpOnly, cookie.isSession, cookie.expiry);

  // Check that the cookie service accepted the new cookie.
  do_check_eq(Services.cookiemgr.countCookiesFromHost(cookie.host), 1);

  // Wait for the cookie service to rename the old database and rebuild.
  new _observer(sub_generator, "cookie-db-rebuilding");
  yield;
  do_execute_soon(function() { do_run_generator(sub_generator); });
  yield;

  // At this point, the cookies should still be in memory.
  do_check_eq(Services.cookiemgr.countCookiesFromHost("foo.com"), 1);
  do_check_eq(Services.cookiemgr.countCookiesFromHost(cookie.host), 1);
  do_check_eq(do_count_cookies(), 2);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Check that the original database was renamed, and that it contains the
  // original cookie.
  do_check_true(do_get_backup_file(profile).exists());
  let backupdb = Services.storage.openDatabase(do_get_backup_file(profile));
  do_check_eq(do_count_cookies_in_db(backupdb, "foo.com"), 1);
  backupdb.close();

  // Load the profile, and check that it contains the new cookie.
  do_load_profile();

  do_check_eq(Services.cookiemgr.countCookiesFromHost("foo.com"), 1);
  let enumerator = Services.cookiemgr.getCookiesFromHost(cookie.host);
  do_check_true(enumerator.hasMoreElements());
  let dbcookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
  do_check_eq(dbcookie.value, "hallo");
  do_check_false(enumerator.hasMoreElements());

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  do_check_false(do_get_cookie_file(profile).exists());
  do_check_false(do_get_backup_file(profile).exists());
  do_run_generator(generator);
}

function run_test_2(generator)
{
  // Load the profile and populate it.
  do_load_profile();
  for (let i = 0; i < 3000; ++i) {
    let uri = NetUtil.newURI("http://" + i + ".com/");
    Services.cookies.setCookieString(uri, null, "oh=hai; max-age=1000", null);
  }

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Corrupt the database file.
  let size = do_corrupt_db(do_get_cookie_file(profile));

  // Load the profile.
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  do_check_false(do_get_backup_file(profile).exists());

  // Synchronously read in the first cookie. This will cause it to go into the
  // cookie table, whereupon it will be written out during database rebuild.
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 1);

  // Wait for the asynchronous read to choke, at which point the backup file
  // will be created and the database rebuilt.
  new _observer(sub_generator, "cookie-db-rebuilding");
  yield;
  do_execute_soon(function() { do_run_generator(sub_generator); });
  yield;

  // At this point, the cookies should still be in memory.
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 1);
  do_check_eq(do_count_cookies(), 1);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Check that the original database was renamed.
  do_check_true(do_get_backup_file(profile).exists());
  do_check_eq(do_get_backup_file(profile).fileSize, size);
  let db = Services.storage.openDatabase(do_get_cookie_file(profile));
  do_check_eq(do_count_cookies_in_db(db, "0.com"), 1);
  db.close();

  // Load the profile, and check that it contains the new cookie.
  do_load_profile();
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 1);
  do_check_eq(do_count_cookies(), 1);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  do_check_false(do_get_cookie_file(profile).exists());
  do_check_false(do_get_backup_file(profile).exists());
  do_run_generator(generator);
}

function run_test_3(generator)
{
  // Set the maximum cookies per base domain limit to a large value, so that
  // corrupting the database is easier.
  Services.prefs.setIntPref("network.cookie.maxPerHost", 3000);

  // Load the profile and populate it.
  do_load_profile();
  for (let i = 0; i < 10; ++i) {
    let uri = NetUtil.newURI("http://hither.com/");
    Services.cookies.setCookieString(uri, null, "oh" + i + "=hai; max-age=1000",
      null);
  }
  for (let i = 10; i < 3000; ++i) {
    let uri = NetUtil.newURI("http://haithur.com/");
    Services.cookies.setCookieString(uri, null, "oh" + i + "=hai; max-age=1000",
      null);
  }

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Corrupt the database file.
  let size = do_corrupt_db(do_get_cookie_file(profile));

  // Load the profile.
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  do_check_false(do_get_backup_file(profile).exists());

  // Synchronously read in the cookies for our two domains. The first should
  // succeed, but the second should fail midway through, resulting in none of
  // those cookies being present.
  do_check_eq(Services.cookiemgr.countCookiesFromHost("hither.com"), 10);
  do_check_eq(Services.cookiemgr.countCookiesFromHost("haithur.com"), 0);

  // Wait for the backup file to be created and the database rebuilt.
  do_check_false(do_get_backup_file(profile).exists());
  new _observer(sub_generator, "cookie-db-rebuilding");
  yield;
  do_execute_soon(function() { do_run_generator(sub_generator); });
  yield;

  // Close the profile.
  do_close_profile(sub_generator);
  yield;
  let db = Services.storage.openDatabase(do_get_cookie_file(profile));
  do_check_eq(do_count_cookies_in_db(db, "hither.com"), 10);
  do_check_eq(do_count_cookies_in_db(db), 10);
  db.close();

  // Check that the original database was renamed.
  do_check_true(do_get_backup_file(profile).exists());
  do_check_eq(do_get_backup_file(profile).fileSize, size);

  // Rename it back, and try loading the entire database synchronously.
  do_get_backup_file(profile).moveTo(null, "cookies.sqlite");
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  do_check_false(do_get_backup_file(profile).exists());

  // Synchronously read in everything.
  do_check_eq(do_count_cookies(), 0);

  // Wait for the backup file to be created and the database rebuilt.
  do_check_false(do_get_backup_file(profile).exists());
  new _observer(sub_generator, "cookie-db-rebuilding");
  yield;
  do_execute_soon(function() { do_run_generator(sub_generator); });
  yield;

  // Close the profile.
  do_close_profile(sub_generator);
  yield;
  let db = Services.storage.openDatabase(do_get_cookie_file(profile));
  do_check_eq(do_count_cookies_in_db(db), 0);
  db.close();

  // Check that the original database was renamed.
  do_check_true(do_get_backup_file(profile).exists());
  do_check_eq(do_get_backup_file(profile).fileSize, size);

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  do_check_false(do_get_cookie_file(profile).exists());
  do_check_false(do_get_backup_file(profile).exists());
  do_run_generator(generator);
}

function run_test_4(generator)
{
  // Load the profile and populate it.
  do_load_profile();
  for (let i = 0; i < 3000; ++i) {
    let uri = NetUtil.newURI("http://" + i + ".com/");
    Services.cookies.setCookieString(uri, null, "oh=hai; max-age=1000", null);
  }

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Corrupt the database file.
  let size = do_corrupt_db(do_get_cookie_file(profile));

  // Load the profile.
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  do_check_false(do_get_backup_file(profile).exists());

  // Synchronously read in the first cookie. This will cause it to go into the
  // cookie table, whereupon it will be written out during database rebuild.
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 1);

  // Queue up an INSERT for the same base domain. This should also go into
  // memory and be written out during database rebuild.
  let uri = NetUtil.newURI("http://0.com/");
  Services.cookies.setCookieString(uri, null, "oh2=hai; max-age=1000", null);

  // Wait for the asynchronous read to choke and the insert to fail shortly
  // thereafter, at which point the backup file will be created and the database
  // rebuilt.
  new _observer(sub_generator, "cookie-db-rebuilding");
  yield;
  do_execute_soon(function() { do_run_generator(sub_generator); });
  yield;

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Check that the original database was renamed.
  do_check_true(do_get_backup_file(profile).exists());
  do_check_eq(do_get_backup_file(profile).fileSize, size);
  let db = Services.storage.openDatabase(do_get_cookie_file(profile));
  do_check_eq(do_count_cookies_in_db(db, "0.com"), 2);
  db.close();

  // Load the profile, and check that it contains the new cookie.
  do_load_profile();
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 2);
  do_check_eq(do_count_cookies(), 2);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  do_check_false(do_get_cookie_file(profile).exists());
  do_check_false(do_get_backup_file(profile).exists());
  do_run_generator(generator);
}

function run_test_4(generator)
{
  // Load the profile and populate it.
  do_load_profile();
  for (let i = 0; i < 3000; ++i) {
    let uri = NetUtil.newURI("http://" + i + ".com/");
    Services.cookies.setCookieString(uri, null, "oh=hai; max-age=1000", null);
  }

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Corrupt the database file.
  let size = do_corrupt_db(do_get_cookie_file(profile));

  // Load the profile.
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  do_check_false(do_get_backup_file(profile).exists());

  // Synchronously read in the first cookie. This will cause it to go into the
  // cookie table, whereupon it will be written out during database rebuild.
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 1);

  // Queue up an INSERT for the same base domain. This should also go into
  // memory and be written out during database rebuild.
  let uri = NetUtil.newURI("http://0.com/");
  Services.cookies.setCookieString(uri, null, "oh2=hai; max-age=1000", null);

  // Wait for the asynchronous read to choke and the insert to fail shortly
  // thereafter, at which point the backup file will be created and the database
  // rebuilt.
  new _observer(sub_generator, "cookie-db-rebuilding");
  yield;
  do_execute_soon(function() { do_run_generator(sub_generator); });
  yield;

  // At this point, the cookies should still be in memory.
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 2);
  do_check_eq(do_count_cookies(), 2);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Check that the original database was renamed.
  do_check_true(do_get_backup_file(profile).exists());
  do_check_eq(do_get_backup_file(profile).fileSize, size);
  let db = Services.storage.openDatabase(do_get_cookie_file(profile));
  do_check_eq(do_count_cookies_in_db(db, "0.com"), 2);
  db.close();

  // Load the profile, and check that it contains the new cookie.
  do_load_profile();
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 2);
  do_check_eq(do_count_cookies(), 2);

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  do_check_false(do_get_cookie_file(profile).exists());
  do_check_false(do_get_backup_file(profile).exists());
  do_run_generator(generator);
}

function run_test_5(generator)
{
  // Load the profile and populate it.
  do_load_profile();
  let uri = NetUtil.newURI("http://bar.com/");
  Services.cookies.setCookieString(uri, null, "oh=hai; path=/; max-age=1000",
    null);
  for (let i = 0; i < 3000; ++i) {
    let uri = NetUtil.newURI("http://" + i + ".com/");
    Services.cookies.setCookieString(uri, null, "oh=hai; max-age=1000", null);
  }

  // Close the profile.
  do_close_profile(sub_generator);
  yield;

  // Corrupt the database file.
  let size = do_corrupt_db(do_get_cookie_file(profile));

  // Load the profile.
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  do_check_false(do_get_backup_file(profile).exists());

  // Synchronously read in the first two cookies. This will cause them to go
  // into the cookie table, whereupon it will be written out during database
  // rebuild.
  do_check_eq(Services.cookiemgr.countCookiesFromHost("bar.com"), 1);
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 1);

  // Wait for the asynchronous read to choke, at which point the backup file
  // will be created and a new connection opened.
  new _observer(sub_generator, "cookie-db-rebuilding");
  yield;

  // At this point, the cookies should still be in memory. (Note that these
  // calls are re-entrant into the cookie service, but it's OK!)
  do_check_eq(Services.cookiemgr.countCookiesFromHost("bar.com"), 1);
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 1);
  do_check_eq(do_count_cookies(), 2);
  do_check_true(do_get_backup_file(profile).exists());
  do_check_eq(do_get_backup_file(profile).fileSize, size);
  do_check_false(do_get_rebuild_backup_file(profile).exists());

  // Open a database connection, and write a row that will trigger a constraint
  // violation.
  let db = new CookieDatabaseConnection(do_get_cookie_file(profile), 4);
  db.insertCookie(cookie);
  do_check_eq(do_count_cookies_in_db(db.db, "bar.com"), 1);
  do_check_eq(do_count_cookies_in_db(db.db), 1);
  db.close();

  // Wait for the rebuild to bail and the database to be closed.
  new _observer(sub_generator, "cookie-db-closed");
  yield;

  // Check that the original backup and the database itself are gone.
  do_check_true(do_get_rebuild_backup_file(profile).exists());
  do_check_true(do_get_backup_file(profile).exists());
  do_check_eq(do_get_backup_file(profile).fileSize, size);
  do_check_false(do_get_cookie_file(profile).exists());

  // Check that the rebuild backup has the original bar.com cookie, and possibly
  // a 0.com cookie depending on whether it got written out first or second.
  db = new CookieDatabaseConnection(do_get_rebuild_backup_file(profile), 4);
  do_check_eq(do_count_cookies_in_db(db.db, "bar.com"), 1);
  let count = do_count_cookies_in_db(db.db);
  do_check_true(count == 1 ||
    count == 2 && do_count_cookies_in_db(db.db, "0.com") == 1);
  db.close();

  do_check_eq(Services.cookiemgr.countCookiesFromHost("bar.com"), 1);
  do_check_eq(Services.cookiemgr.countCookiesFromHost("0.com"), 1);
  do_check_eq(do_count_cookies(), 2);

  // Close the profile. We do not need to wait for completion, because the
  // database has already been closed.
  do_close_profile();

  // Clean up.
  do_get_backup_file(profile).remove(false);
  do_get_rebuild_backup_file(profile).remove(false);
  do_check_false(do_get_cookie_file(profile).exists());
  do_check_false(do_get_backup_file(profile).exists());
  do_check_false(do_get_rebuild_backup_file(profile).exists());
  do_run_generator(generator);
}

