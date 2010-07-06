/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  do_load_manifest("cookieprompt.manifest");

  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  var pb = null;
  try {
    pb = Cc["@mozilla.org/privatebrowsing;1"].getService(Ci.nsIPrivateBrowsingService);
  } catch (e) {}

  var spec = "http://foo.bar/baz";
  var uri = ios.newURI(spec, null, null);

  // accept all cookies
  prefs.setIntPref("network.cookie.lifetimePolicy", 0);
  // add a test cookie
  cs.setCookieString(uri, null, "foo=bar", null);
  do_check_eq(cs.countCookiesFromHost("foo.bar"), 1);
  // ask all cookies (will result in rejection because the prompt is not available)
  prefs.setIntPref("network.cookie.lifetimePolicy", 1);
  // add a test cookie
  cs.setCookieString(uri, null, "bar=baz", null);
  do_check_eq(cs.countCookiesFromHost("foo.bar"), 1);
  cs.removeAll();

  // if private browsing is available
  if (pb) {
    // enter private browsing mode
    pb.privateBrowsingEnabled = true;

    // accept all cookies
    prefs.setIntPref("network.cookie.lifetimePolicy", 0);
    // add a test cookie
    cs.setCookieString(uri, null, "foobar=bar", null);
    do_check_eq(cs.countCookiesFromHost("foo.bar"), 1);
    // ask all cookies (will result in rejection because the prompt is not available)
    prefs.setIntPref("network.cookie.lifetimePolicy", 1);
    // add a test cookie
    cs.setCookieString(uri, null, "foobaz=bar", null);
    do_check_eq(cs.countCookiesFromHost("foo.bar"), 2);
  }
}

