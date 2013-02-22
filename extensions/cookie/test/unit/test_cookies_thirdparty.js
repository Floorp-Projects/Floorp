/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// test third party cookie blocking, for the cases:
// 1) with null channel
// 2) with channel, but with no docshell parent

function run_test() {
  // Create URIs and channels pointing to foo.com and bar.com.
  // We will use these to put foo.com into first and third party contexts.
  var spec1 = "http://foo.com/foo.html";
  var spec2 = "http://bar.com/bar.html";
  var uri1 = NetUtil.newURI(spec1);
  var uri2 = NetUtil.newURI(spec2);
  var channel1 = NetUtil.newChannel(uri1);
  var channel2 = NetUtil.newChannel(uri2);

  // test with cookies enabled
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  do_set_cookies(uri1, channel1, true, [1, 2, 3, 4]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [1, 2, 3, 4]);
  Services.cookies.removeAll();

  // test with third party cookies blocked
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 1);
  do_set_cookies(uri1, channel1, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();

  // Force the channel URI to be used when determining the originating URI of
  // the channel.
  var httpchannel1 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
  var httpchannel2 = channel2.QueryInterface(Ci.nsIHttpChannelInternal);
  httpchannel1.forceAllowThirdPartyCookie = true;
  httpchannel2.forceAllowThirdPartyCookie = true;

  // test with cookies enabled
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  do_set_cookies(uri1, channel1, true, [1, 2, 3, 4]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [1, 2, 3, 4]);
  Services.cookies.removeAll();

  // test with third party cookies blocked
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 1);
  do_set_cookies(uri1, channel1, true, [0, 1, 1, 2]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();

  // test with third party cookies limited
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 3);
  do_set_cookies(uri1, channel1, true, [0, 1, 2, 3]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();
  do_set_single_http_cookie(uri1, channel1, 1);
  do_set_cookies(uri1, channel2, true, [2, 3, 4, 5]);
  Services.cookies.removeAll();

  // Test per-site 3rd party cookie blocking with cookies enabled
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  var kPermissionType = "cookie";
  var ALLOW_FIRST_PARTY_ONLY = 9;
  // ALLOW_FIRST_PARTY_ONLY overrides
  Services.permissions.add(uri1, kPermissionType, ALLOW_FIRST_PARTY_ONLY);
  do_set_cookies(uri1, channel1, true, [0, 1, 1, 2]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();

  // Test per-site 3rd party cookie blocking with 3rd party cookies disabled
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 1);
  do_set_cookies(uri1, channel1, true, [0, 1, 1, 2]);
  Services.cookies.removeAll();
  // No preference has been set for uri2, but it should act as if
  // ALLOW_FIRST_PARTY_ONLY has been set
  do_set_cookies(uri2, channel2, true, [0, 1, 1, 2]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();

  // Test per-site 3rd party cookie blocking with 3rd party cookies limited
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 3);
  do_set_cookies(uri1, channel1, true, [0, 1, 1, 2]);
  Services.cookies.removeAll();
  // No preference has been set for uri2, but it should act as if
  // LIMIT_THIRD_PARTY has been set
  do_set_cookies(uri2, channel2, true, [0, 1, 2, 3]);
  Services.cookies.removeAll();
  do_set_single_http_cookie(uri2, channel2, 1);
  do_set_cookies(uri2, channel2, true, [2, 3, 4, 5]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();
  do_set_single_http_cookie(uri1, channel1, 1);
  do_set_cookies(uri1, channel2, true, [1, 1, 1, 1]);
  Services.cookies.removeAll();

  // Test per-site 3rd party cookie limiting with cookies enabled
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  var kPermissionType = "cookie";
  var LIMIT_THIRD_PARTY = 10;
  // LIMIT_THIRD_PARTY overrides
  Services.permissions.add(uri1, kPermissionType, LIMIT_THIRD_PARTY);
  do_set_cookies(uri1, channel1, true, [0, 1, 2, 3]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();
  do_set_single_http_cookie(uri1, channel1, 1);
  do_set_cookies(uri1, channel2, true, [2, 3, 4, 5]);
  Services.cookies.removeAll();

  // Test per-site 3rd party cookie limiting with 3rd party cookies disabled
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 1);
  do_set_cookies(uri1, channel1, true, [0, 1, 2, 3]);
  Services.cookies.removeAll();
  // No preference has been set for uri2, but it should act as if
  // ALLOW_FIRST_PARTY_ONLY has been set
  do_set_cookies(uri2, channel2, true, [0, 1, 1, 2]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();
  do_set_single_http_cookie(uri1, channel1, 1);
  do_set_cookies(uri1, channel2, true, [2, 3, 4, 5]);
  Services.cookies.removeAll();

  // Test per-site 3rd party cookie limiting with 3rd party cookies limited
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 3);
  do_set_cookies(uri1, channel1, true, [0, 1, 2, 3]);
  Services.cookies.removeAll();
  // No preference has been set for uri2, but it should act as if
  // LIMIT_THIRD_PARTY has been set
  do_set_cookies(uri2, channel2, true, [0, 1, 2, 3]);
  Services.cookies.removeAll();
  do_set_single_http_cookie(uri2, channel2, 1);
  do_set_cookies(uri2, channel2, true, [2, 3, 4, 5]);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, true, [0, 0, 0, 0]);
  Services.cookies.removeAll();
  do_set_single_http_cookie(uri1, channel1, 1);
  do_set_cookies(uri1, channel2, true, [2, 3, 4, 5]);
  Services.cookies.removeAll();
}

