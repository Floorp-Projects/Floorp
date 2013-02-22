/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);
  var expiry = (Date.now() + 1000) * 1000;

  cm.removeAll();

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  // test that variants of 'baz.com' get normalized appropriately, but that
  // malformed hosts are rejected
  cm.add("baz.com", "/", "foo", "bar", false, false, true, expiry);
  do_check_eq(cm.countCookiesFromHost("baz.com"), 1);
  do_check_eq(cm.countCookiesFromHost("BAZ.com"), 1);
  do_check_eq(cm.countCookiesFromHost(".baz.com"), 1);
  do_check_eq(cm.countCookiesFromHost("baz.com."), 0);
  do_check_eq(cm.countCookiesFromHost(".baz.com."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost("baz.com..");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost("baz..com");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost("..baz.com");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  cm.remove("BAZ.com.", "foo", "/", false);
  do_check_eq(cm.countCookiesFromHost("baz.com"), 1);
  cm.remove("baz.com", "foo", "/", false);
  do_check_eq(cm.countCookiesFromHost("baz.com"), 0);

  // Test that 'baz.com' and 'baz.com.' are treated differently
  cm.add("baz.com.", "/", "foo", "bar", false, false, true, expiry);
  do_check_eq(cm.countCookiesFromHost("baz.com"), 0);
  do_check_eq(cm.countCookiesFromHost("BAZ.com"), 0);
  do_check_eq(cm.countCookiesFromHost(".baz.com"), 0);
  do_check_eq(cm.countCookiesFromHost("baz.com."), 1);
  do_check_eq(cm.countCookiesFromHost(".baz.com."), 1);
  cm.remove("baz.com", "foo", "/", false);
  do_check_eq(cm.countCookiesFromHost("baz.com."), 1);
  cm.remove("baz.com.", "foo", "/", false);
  do_check_eq(cm.countCookiesFromHost("baz.com."), 0);

  // test that domain cookies are illegal for IP addresses, aliases such as
  // 'localhost', and eTLD's such as 'co.uk'
  cm.add("192.168.0.1", "/", "foo", "bar", false, false, true, expiry);
  do_check_eq(cm.countCookiesFromHost("192.168.0.1"), 1);
  do_check_eq(cm.countCookiesFromHost("192.168.0.1."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".192.168.0.1");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost(".192.168.0.1.");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.add("localhost", "/", "foo", "bar", false, false, true, expiry);
  do_check_eq(cm.countCookiesFromHost("localhost"), 1);
  do_check_eq(cm.countCookiesFromHost("localhost."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".localhost");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost(".localhost.");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.add("co.uk", "/", "foo", "bar", false, false, true, expiry);
  do_check_eq(cm.countCookiesFromHost("co.uk"), 1);
  do_check_eq(cm.countCookiesFromHost("co.uk."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".co.uk");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost(".co.uk.");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.removeAll();

  // test that setting an empty or '.' http:// host results in a no-op
  var uri = NetUtil.newURI("http://baz.com/");
  var emptyuri = NetUtil.newURI("http:///");
  var doturi = NetUtil.newURI("http://./");
  do_check_eq(uri.asciiHost, "baz.com");
  do_check_eq(emptyuri.asciiHost, "");
  do_check_eq(doturi.asciiHost, ".");
  cs.setCookieString(emptyuri, null, "foo2=bar", null);
  do_check_eq(getCookieCount(), 0);
  cs.setCookieString(doturi, null, "foo3=bar", null);
  do_check_eq(getCookieCount(), 0);
  cs.setCookieString(uri, null, "foo=bar", null);
  do_check_eq(getCookieCount(), 1);

  do_check_eq(cs.getCookieString(uri, null), "foo=bar");
  do_check_eq(cs.getCookieString(emptyuri, null), null);
  do_check_eq(cs.getCookieString(doturi, null), null);

  do_check_eq(cm.countCookiesFromHost(""), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost("..");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  var e = cm.getCookiesFromHost("");
  do_check_false(e.hasMoreElements());
  do_check_throws(function() {
    cm.getCookiesFromHost(".");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.getCookiesFromHost("..");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  e = cm.getCookiesFromHost("baz.com");
  do_check_true(e.hasMoreElements());
  do_check_eq(e.getNext().QueryInterface(Ci.nsICookie2).name, "foo");
  do_check_false(e.hasMoreElements());
  e = cm.getCookiesFromHost("");
  do_check_false(e.hasMoreElements());
  do_check_throws(function() {
    cm.getCookiesFromHost(".");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.getCookiesFromHost("..");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.removeAll();

  // test that an empty file:// host works
  emptyuri = NetUtil.newURI("file:///");
  do_check_eq(emptyuri.asciiHost, "");
  do_check_eq(NetUtil.newURI("file://./").asciiHost, "");
  do_check_eq(NetUtil.newURI("file://foo.bar/").asciiHost, "");
  cs.setCookieString(emptyuri, null, "foo2=bar", null);
  do_check_eq(getCookieCount(), 1);
  cs.setCookieString(emptyuri, null, "foo3=bar; domain=", null);
  do_check_eq(getCookieCount(), 2);
  cs.setCookieString(emptyuri, null, "foo4=bar; domain=.", null);
  do_check_eq(getCookieCount(), 2);
  cs.setCookieString(emptyuri, null, "foo5=bar; domain=bar.com", null);
  do_check_eq(getCookieCount(), 2);

  do_check_eq(cs.getCookieString(emptyuri, null), "foo2=bar; foo3=bar");

  do_check_eq(cm.countCookiesFromHost("baz.com"), 0);
  do_check_eq(cm.countCookiesFromHost(""), 2);
  do_check_throws(function() {
    cm.countCookiesFromHost(".");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  e = cm.getCookiesFromHost("baz.com");
  do_check_false(e.hasMoreElements());
  e = cm.getCookiesFromHost("");
  do_check_true(e.hasMoreElements());
  e.getNext();
  do_check_true(e.hasMoreElements());
  e.getNext();
  do_check_false(e.hasMoreElements());
  do_check_throws(function() {
    cm.getCookiesFromHost(".");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.removeAll();

  // test that an empty host to add() or remove() works,
  // but a host of '.' doesn't
  cm.add("", "/", "foo2", "bar", false, false, true, expiry);
  do_check_eq(getCookieCount(), 1);
  do_check_throws(function() {
    cm.add(".", "/", "foo3", "bar", false, false, true, expiry);
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_eq(getCookieCount(), 1);

  cm.remove("", "foo2", "/", false);
  do_check_eq(getCookieCount(), 0);
  do_check_throws(function() {
    cm.remove(".", "foo3", "/", false);
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  // test that the 'domain' attribute accepts a leading dot for IP addresses,
  // aliases such as 'localhost', and eTLD's such as 'co.uk'; but that the
  // resulting cookie is for the exact host only.
  testDomainCookie("http://192.168.0.1/", "192.168.0.1");
  testDomainCookie("http://localhost/", "localhost");
  testDomainCookie("http://co.uk/", "co.uk");

  // Test that trailing dots are treated differently for purposes of the
  // 'domain' attribute when using setCookieString.
  testTrailingDotCookie("http://192.168.0.1", "192.168.0.1");
  testTrailingDotCookie("http://localhost", "localhost");
  testTrailingDotCookie("http://foo.com", "foo.com");

  cm.removeAll();
}

function getCookieCount() {
  var count = 0;
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);
  var enumerator = cm.enumerator;
  while (enumerator.hasMoreElements()) {
    if (!(enumerator.getNext() instanceof Ci.nsICookie2))
      throw new Error("not a cookie");
    ++count;
  }
  return count;
}

function testDomainCookie(uriString, domain) {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);

  cm.removeAll();

  var uri = NetUtil.newURI(uriString);
  cs.setCookieString(uri, null, "foo=bar; domain=" + domain, null);
  var e = cm.getCookiesFromHost(domain);
  do_check_true(e.hasMoreElements());
  do_check_eq(e.getNext().QueryInterface(Ci.nsICookie2).host, domain);
  cm.removeAll();

  cs.setCookieString(uri, null, "foo=bar; domain=." + domain, null);
  e = cm.getCookiesFromHost(domain);
  do_check_true(e.hasMoreElements());
  do_check_eq(e.getNext().QueryInterface(Ci.nsICookie2).host, domain);
  cm.removeAll();
}

function testTrailingDotCookie(uriString, domain) {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);

  cm.removeAll();

  var uri = NetUtil.newURI(uriString);
  cs.setCookieString(uri, null, "foo=bar; domain=" + domain + ".", null);
  do_check_eq(cm.countCookiesFromHost(domain), 0);
  do_check_eq(cm.countCookiesFromHost(domain + "."), 0);
  cm.removeAll();

  uri = NetUtil.newURI(uriString + ".");
  cs.setCookieString(uri, null, "foo=bar; domain=" + domain, null);
  do_check_eq(cm.countCookiesFromHost(domain), 0);
  do_check_eq(cm.countCookiesFromHost(domain + "."), 0);
  cm.removeAll();
}

