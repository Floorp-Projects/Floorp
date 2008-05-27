// test third party cookie blocking, for the cases:
// 1) with null channel
// 2) with channel, but with no docshell parent

const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  var spec = "http://foo.com/dribble.html";
  var uri = ios.newURI(spec, null, null);
  var channel = ios.newChannelFromURI(uri);

  // test with cookies enabled
  prefs.setIntPref("network.cookie.cookieBehavior", 0);
  // without channel
  cs.setCookieString(uri, null, "oh=hai", null);
  do_check_eq(cs.countCookiesFromHost("foo.com"), 1);
  // with channel
  cs.setCookieString(uri, null, "can=has", channel);
  do_check_eq(cs.countCookiesFromHost("foo.com"), 2);
  // without channel, from http
  cs.setCookieStringFromHttp(uri, null, null, "cheez=burger", null, null);
  do_check_eq(cs.countCookiesFromHost("foo.com"), 3);
  // with channel, from http
  cs.setCookieStringFromHttp(uri, null, null, "hot=dog", null, channel);
  do_check_eq(cs.countCookiesFromHost("foo.com"), 4);
  cs.removeAll();

  // test with third party cookies blocked
  prefs.setIntPref("network.cookie.cookieBehavior", 1);
  // without channel
  cs.setCookieString(uri, null, "oh=hai", null);
  do_check_eq(cs.countCookiesFromHost("foo.com"), 0);
  // with channel
  cs.setCookieString(uri, null, "can=has", channel);
  do_check_eq(cs.countCookiesFromHost("foo.com"), 0);
  // without channel, from http
  cs.setCookieStringFromHttp(uri, null, null, "cheez=burger", null, null);
  do_check_eq(cs.countCookiesFromHost("foo.com"), 0);
  // with channel, from http
  cs.setCookieStringFromHttp(uri, null, null, "hot=dog", null, channel);
  do_check_eq(cs.countCookiesFromHost("foo.com"), 0);
}

