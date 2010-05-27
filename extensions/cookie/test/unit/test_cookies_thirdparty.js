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

  // Create URIs and channels pointing to foo.com and bar.com.
  // We will use these to put foo.com into first and third party contexts.
  var spec1 = "http://foo.com/foo.html";
  var spec2 = "http://bar.com/bar.html";
  var uri1 = ios.newURI(spec1, null, null);
  var uri2 = ios.newURI(spec2, null, null);
  var channel1 = ios.newChannelFromURI(uri1);
  var channel2 = ios.newChannelFromURI(uri2);

  // test with cookies enabled
  prefs.setIntPref("network.cookie.cookieBehavior", 0);
  run_cookie_test(cs, uri1, channel1, [1, 2, 3, 4]);
  run_cookie_test(cs, uri1, channel2, [1, 2, 3, 4]);

  // test with third party cookies blocked
  prefs.setIntPref("network.cookie.cookieBehavior", 1);
  run_cookie_test(cs, uri1, channel1, [0, 0, 0, 0]);
  run_cookie_test(cs, uri1, channel2, [0, 0, 0, 0]);

  // Force the channel URI to be used when determining the originating URI of
  // the channel.
  var httpchannel1 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
  var httpchannel2 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
  httpchannel1.forceAllowThirdPartyCookie = true;
  httpchannel2.forceAllowThirdPartyCookie = true;

  // test with cookies enabled
  prefs.setIntPref("network.cookie.cookieBehavior", 0);
  run_cookie_test(cs, uri1, channel1, [1, 2, 3, 4]);
  run_cookie_test(cs, uri1, channel2, [1, 2, 3, 4]);

  // test with third party cookies blocked
  prefs.setIntPref("network.cookie.cookieBehavior", 1);
  run_cookie_test(cs, uri1, channel1, [0, 1, 1, 2]);
  run_cookie_test(cs, uri1, channel2, [0, 0, 0, 0]);
}

function run_cookie_test(cs, uri, channel, expected) {
  // without channel
  cs.setCookieString(uri, null, "oh=hai", null);
  do_check_eq(cs.countCookiesFromHost("foo.com"), expected[0]);
  // with channel
  cs.setCookieString(uri, null, "can=has", channel);
  do_check_eq(cs.countCookiesFromHost("foo.com"), expected[1]);
  // without channel, from http
  cs.setCookieStringFromHttp(uri, null, null, "cheez=burger", null, null);
  do_check_eq(cs.countCookiesFromHost("foo.com"), expected[2]);
  // with channel, from http
  cs.setCookieStringFromHttp(uri, null, null, "hot=dog", null, channel);
  do_check_eq(cs.countCookiesFromHost("foo.com"), expected[3]);
  cs.removeAll();
}

