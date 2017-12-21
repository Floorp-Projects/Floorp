/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/NetUtil.jsm");

function run_test() {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  var expiry = (Date.now() + 1000) * 1000;

  // Test our handling of host names with a single character at the beginning
  // followed by a dot.
  cm.add("e.mail.com", "/", "foo", "bar", false, false, true, expiry, {});
  Assert.equal(cm.countCookiesFromHost("e.mail.com"), 1);
  Assert.equal(cs.getCookieString(NetUtil.newURI("http://e.mail.com"), null), "foo=bar");
}
