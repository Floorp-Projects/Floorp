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
  var httpchannel2 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
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
}

