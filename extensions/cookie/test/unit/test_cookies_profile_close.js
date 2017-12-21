/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the cookie APIs behave sanely after 'profile-before-change'.

var test_generator = do_run_test();

function run_test() {
  do_test_pending();
  test_generator.next();
}

function finish_test() {
  executeSoon(function() {
    test_generator.return();
    do_test_finished();
  });
}

function* do_run_test() {
  // Set up a profile.
  let profile = do_get_profile();

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  // Start the cookieservice.
  Services.cookies;

  // Set a cookie.
  let uri = NetUtil.newURI("http://foo.com");
  Services.cookies.setCookieString(uri, null, "oh=hai; max-age=1000", null);
  let enumerator = Services.cookiemgr.enumerator;
  Assert.ok(enumerator.hasMoreElements());
  let cookie = enumerator.getNext();
  Assert.ok(!enumerator.hasMoreElements());

  // Fire 'profile-before-change'.
  do_close_profile();

  // Check that the APIs behave appropriately.
  Assert.equal(Services.cookies.getCookieString(uri, null), null);
  Assert.equal(Services.cookies.getCookieStringFromHttp(uri, null, null), null);
  Services.cookies.setCookieString(uri, null, "oh2=hai", null);
  Services.cookies.setCookieStringFromHttp(uri, null, null, "oh3=hai", null, null);
  Assert.equal(Services.cookies.getCookieString(uri, null), null);

  do_check_throws(function() {
    Services.cookiemgr.removeAll();
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookiemgr.enumerator;
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookiemgr.add("foo.com", "", "oh4", "hai", false, false, false, 0, {});
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookiemgr.remove("foo.com", "", "oh4", false, {});
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    let file = profile.clone();
    file.append("cookies.txt");
    Services.cookiemgr.importCookies(file);
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookiemgr.cookieExists(cookie);
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookies.countCookiesFromHost("foo.com");
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  do_check_throws(function() {
    Services.cookies.getCookiesFromHost("foo.com", {});
  }, Cr.NS_ERROR_NOT_AVAILABLE);

  // Wait for the database to finish closing.
  new _observer(test_generator, "cookie-db-closed");
  yield;

  // Load the profile and check that the API is available.
  do_load_profile();
  Assert.ok(Services.cookiemgr.cookieExists(cookie));

  finish_test();
}

