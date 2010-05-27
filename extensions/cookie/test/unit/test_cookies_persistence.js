// test for cookie persistence across sessions, for the cases:
// 1) network.cookie.lifetimePolicy = 0 (expire naturally)
// 2) network.cookie.lifetimePolicy = 2 (expire at end of session)

const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {
  // Set up a profile.
  do_get_profile();

  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cso = cs.QueryInterface(Ci.nsIObserver);
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

  // Force the channel URI to be used when determining the originating URI of
  // the channel.
  var httpchannel1 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
  var httpchannel2 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
  httpchannel1.forceAllowThirdPartyCookie = true;
  httpchannel2.forceAllowThirdPartyCookie = true;

  // test with cookies enabled, and third party cookies persistent.
  prefs.setIntPref("network.cookie.cookieBehavior", 0);
  prefs.setBoolPref("network.cookie.thirdparty.sessionOnly", false);
  do_set_cookies(uri1, channel1, false, [1, 2, 3, 4]);
  do_set_cookies(uri2, channel2, true, [1, 2, 3, 4]);

  // fake a profile change
  do_reload_profile(cso);
  do_check_eq(cs.countCookiesFromHost(uri1.host), 4);
  do_check_eq(cs.countCookiesFromHost(uri2.host), 0);

  // cleanse them
  do_reload_profile(cso, "shutdown-cleanse");
  do_check_eq(cs.countCookiesFromHost(uri1.host), 0);
  do_check_eq(cs.countCookiesFromHost(uri2.host), 0);

  // test with cookies set to session-only
  prefs.setIntPref("network.cookie.lifetimePolicy", 2);
  do_set_cookies(uri1, channel1, false, [1, 2, 3, 4]);
  do_set_cookies(uri2, channel2, true, [1, 2, 3, 4]);

  // fake a profile change
  do_reload_profile(cso);
  do_check_eq(cs.countCookiesFromHost(uri1.host), 0);
  do_check_eq(cs.countCookiesFromHost(uri2.host), 0);
}

