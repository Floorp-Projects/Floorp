/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// test third party persistence across sessions, for the cases:
// 1) network.cookie.thirdparty.sessionOnly = false
// 2) network.cookie.thirdparty.sessionOnly = true

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

  // Create URIs and channels pointing to foo.com and bar.com.
  // We will use these to put foo.com into first and third party contexts.
  var spec1 = "http://foo.com/foo.html";
  var spec2 = "http://bar.com/bar.html";
  var uri1 = NetUtil.newURI(spec1);
  var uri2 = NetUtil.newURI(spec2);
  var channel1 = NetUtil.newChannel({uri: uri1, loadUsingSystemPrincipal: true});
  var channel2 = NetUtil.newChannel({uri: uri2, loadUsingSystemPrincipal: true});

  // Force the channel URI to be used when determining the originating URI of
  // the channel.
  var httpchannel1 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
  var httpchannel2 = channel2.QueryInterface(Ci.nsIHttpChannelInternal);
  httpchannel1.forceAllowThirdPartyCookie = true;
  httpchannel2.forceAllowThirdPartyCookie = true;

  // test with cookies enabled, and third party cookies persistent.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref("network.cookie.thirdparty.sessionOnly", false);
  do_set_cookies(uri1, channel2, false, [1, 2, 3, 4]);
  do_set_cookies(uri2, channel1, true, [1, 2, 3, 4]);

  // fake a profile change
  do_close_profile(test_generator);
  yield;
  do_load_profile();
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 4);
  Assert.equal(Services.cookies.countCookiesFromHost(uri2.host), 0);

  // test with third party cookies for session only.
  Services.prefs.setBoolPref("network.cookie.thirdparty.sessionOnly", true);
  Services.cookies.removeAll();
  do_set_cookies(uri1, channel2, false, [1, 2, 3, 4]);
  do_set_cookies(uri2, channel1, true, [1, 2, 3, 4]);

  // fake a profile change
  do_close_profile(test_generator);
  yield;
  do_load_profile();
  Assert.equal(Services.cookies.countCookiesFromHost(uri1.host), 0);
  Assert.equal(Services.cookies.countCookiesFromHost(uri2.host), 0);

  finish_test();
}
