/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// test cookie database asynchronous read operation.

let test_generator = do_run_test();

function run_test() {
  do_test_pending();
  test_generator.next();
}

function finish_test() {
  do_execute_soon(function() {
    test_generator.close();
    do_test_finished();
  });
}

function do_run_test() {
  // Set up a profile.
  let profile = do_get_profile();

  // Start the cookieservice, to force creation of a database.
  Services.cookies;

  // Open a database connection now, after synchronous initialization has
  // completed. We may not be able to open one later once asynchronous writing
  // begins.
  do_check_true(do_get_cookie_file(profile).exists());
  let db = new CookieDatabaseConnection(profile, 4);

  for (let i = 0; i < 3000; ++i) {
    let uri = NetUtil.newURI("http://" + i + ".com/");
    Services.cookies.setCookieString(uri, null, "oh=hai; max-age=1000", null);
  }

  do_check_eq(do_count_cookies(), 3000);

  // Wait until all 3000 cookies have been written out to the database.
  while (do_count_cookies_in_db(db.db) < 3000) {
    do_execute_soon(function() {
      do_run_generator(test_generator);
    });
    yield;
  }

  // Check the WAL file size. We set it to 16 pages of 32k, which means it
  // should be around 500k.
  let file = db.db.databaseFile;
  do_check_true(file.exists());
  do_check_true(file.fileSize < 1e6);
  db.close();

  // fake a profile change
  do_close_profile(test_generator);
  yield;
  do_load_profile();

  // test a few random cookies
  do_check_eq(Services.cookiemgr.countCookiesFromHost("2000.com"), 1);
  do_check_eq(Services.cookiemgr.countCookiesFromHost("abc.com"), 0);
  do_check_eq(Services.cookiemgr.countCookiesFromHost("100.com"), 1);
  do_check_eq(Services.cookiemgr.countCookiesFromHost("1400.com"), 1);
  do_check_eq(Services.cookiemgr.countCookiesFromHost("xyz.com"), 0);

  // force synchronous load of everything
  do_check_eq(do_count_cookies(), 3000);

  // check that everything's precisely correct
  for (let i = 0; i < 3000; ++i) {
    let host = i.toString() + ".com";
    do_check_eq(Services.cookiemgr.countCookiesFromHost(host), 1);
  }

  // reload again, to make sure the additions were written correctly
  do_close_profile(test_generator);
  yield;
  do_load_profile();

  // remove some of the cookies, in both reverse and forward order
  for (let i = 100; i-- > 0; ) {
    let host = i.toString() + ".com";
    Services.cookiemgr.remove(host, "oh", "/", false);
  }
  for (let i = 2900; i < 3000; ++i) {
    let host = i.toString() + ".com";
    Services.cookiemgr.remove(host, "oh", "/", false);
  }

  // check the count
  do_check_eq(do_count_cookies(), 2800);

  // reload again, to make sure the removals were written correctly
  do_close_profile(test_generator);
  yield;
  do_load_profile();

  // check the count
  do_check_eq(do_count_cookies(), 2800);

  // reload again, but wait for async read completion
  do_close_profile(test_generator);
  yield;
  do_load_profile(test_generator);
  yield;

  // check that everything's precisely correct
  do_check_eq(do_count_cookies(), 2800);
  for (let i = 100; i < 2900; ++i) {
    let host = i.toString() + ".com";
    do_check_eq(Services.cookiemgr.countCookiesFromHost(host), 1);
  }

  finish_test();
}

