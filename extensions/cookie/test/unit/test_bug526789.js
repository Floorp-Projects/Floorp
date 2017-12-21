/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  var expiry = (Date.now() + 1000) * 1000;

  cm.removeAll();

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  // test that variants of 'baz.com' get normalized appropriately, but that
  // malformed hosts are rejected
  cm.add("baz.com", "/", "foo", "bar", false, false, true, expiry, {});
  Assert.equal(cm.countCookiesFromHost("baz.com"), 1);
  Assert.equal(cm.countCookiesFromHost("BAZ.com"), 1);
  Assert.equal(cm.countCookiesFromHost(".baz.com"), 1);
  Assert.equal(cm.countCookiesFromHost("baz.com."), 0);
  Assert.equal(cm.countCookiesFromHost(".baz.com."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost("baz.com..");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost("baz..com");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost("..baz.com");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  cm.remove("BAZ.com.", "foo", "/", false, {});
  Assert.equal(cm.countCookiesFromHost("baz.com"), 1);
  cm.remove("baz.com", "foo", "/", false, {});
  Assert.equal(cm.countCookiesFromHost("baz.com"), 0);

  // Test that 'baz.com' and 'baz.com.' are treated differently
  cm.add("baz.com.", "/", "foo", "bar", false, false, true, expiry, {});
  Assert.equal(cm.countCookiesFromHost("baz.com"), 0);
  Assert.equal(cm.countCookiesFromHost("BAZ.com"), 0);
  Assert.equal(cm.countCookiesFromHost(".baz.com"), 0);
  Assert.equal(cm.countCookiesFromHost("baz.com."), 1);
  Assert.equal(cm.countCookiesFromHost(".baz.com."), 1);
  cm.remove("baz.com", "foo", "/", false, {});
  Assert.equal(cm.countCookiesFromHost("baz.com."), 1);
  cm.remove("baz.com.", "foo", "/", false, {});
  Assert.equal(cm.countCookiesFromHost("baz.com."), 0);

  // test that domain cookies are illegal for IP addresses, aliases such as
  // 'localhost', and eTLD's such as 'co.uk'
  cm.add("192.168.0.1", "/", "foo", "bar", false, false, true, expiry, {});
  Assert.equal(cm.countCookiesFromHost("192.168.0.1"), 1);
  Assert.equal(cm.countCookiesFromHost("192.168.0.1."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".192.168.0.1");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost(".192.168.0.1.");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.add("localhost", "/", "foo", "bar", false, false, true, expiry, {});
  Assert.equal(cm.countCookiesFromHost("localhost"), 1);
  Assert.equal(cm.countCookiesFromHost("localhost."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".localhost");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost(".localhost.");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.add("co.uk", "/", "foo", "bar", false, false, true, expiry, {});
  Assert.equal(cm.countCookiesFromHost("co.uk"), 1);
  Assert.equal(cm.countCookiesFromHost("co.uk."), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".co.uk");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost(".co.uk.");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.removeAll();

  var uri = NetUtil.newURI("http://baz.com/");
  Assert.equal(uri.asciiHost, "baz.com");
  cs.setCookieString(uri, null, "foo=bar", null);
  Assert.equal(cs.getCookieString(uri, null), "foo=bar");

  Assert.equal(cm.countCookiesFromHost(""), 0);
  do_check_throws(function() {
    cm.countCookiesFromHost(".");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.countCookiesFromHost("..");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  var e = cm.getCookiesFromHost("", {});
  Assert.ok(!e.hasMoreElements());
  do_check_throws(function() {
    cm.getCookiesFromHost(".", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.getCookiesFromHost("..", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  e = cm.getCookiesFromHost("baz.com", {});
  Assert.ok(e.hasMoreElements());
  Assert.equal(e.getNext().QueryInterface(Ci.nsICookie2).name, "foo");
  Assert.ok(!e.hasMoreElements());
  e = cm.getCookiesFromHost("", {});
  Assert.ok(!e.hasMoreElements());
  do_check_throws(function() {
    cm.getCookiesFromHost(".", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  do_check_throws(function() {
    cm.getCookiesFromHost("..", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.removeAll();

  // test that an empty file:// host works
  emptyuri = NetUtil.newURI("file:///");
  Assert.equal(emptyuri.asciiHost, "");
  Assert.equal(NetUtil.newURI("file://./").asciiHost, "");
  Assert.equal(NetUtil.newURI("file://foo.bar/").asciiHost, "");
  cs.setCookieString(emptyuri, null, "foo2=bar", null);
  Assert.equal(getCookieCount(), 1);
  cs.setCookieString(emptyuri, null, "foo3=bar; domain=", null);
  Assert.equal(getCookieCount(), 2);
  cs.setCookieString(emptyuri, null, "foo4=bar; domain=.", null);
  Assert.equal(getCookieCount(), 2);
  cs.setCookieString(emptyuri, null, "foo5=bar; domain=bar.com", null);
  Assert.equal(getCookieCount(), 2);

  Assert.equal(cs.getCookieString(emptyuri, null), "foo2=bar; foo3=bar");

  Assert.equal(cm.countCookiesFromHost("baz.com"), 0);
  Assert.equal(cm.countCookiesFromHost(""), 2);
  do_check_throws(function() {
    cm.countCookiesFromHost(".");
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  e = cm.getCookiesFromHost("baz.com", {});
  Assert.ok(!e.hasMoreElements());
  e = cm.getCookiesFromHost("", {});
  Assert.ok(e.hasMoreElements());
  e.getNext();
  Assert.ok(e.hasMoreElements());
  e.getNext();
  Assert.ok(!e.hasMoreElements());
  do_check_throws(function() {
    cm.getCookiesFromHost(".", {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  cm.removeAll();

  // test that an empty host to add() or remove() works,
  // but a host of '.' doesn't
  cm.add("", "/", "foo2", "bar", false, false, true, expiry, {});
  Assert.equal(getCookieCount(), 1);
  do_check_throws(function() {
    cm.add(".", "/", "foo3", "bar", false, false, true, expiry, {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
  Assert.equal(getCookieCount(), 1);

  cm.remove("", "foo2", "/", false, {});
  Assert.equal(getCookieCount(), 0);
  do_check_throws(function() {
    cm.remove(".", "foo3", "/", false, {});
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  // test that the 'domain' attribute accepts a leading dot for IP addresses,
  // aliases such as 'localhost', and eTLD's such as 'co.uk'; but that the
  // resulting cookie is for the exact host only.
  testDomainCookie("http://192.168.0.1/", "192.168.0.1");
  testDomainCookie("http://localhost/", "localhost");
  testDomainCookie("http://co.uk/", "co.uk");

  // Test that trailing dots are treated differently for purposes of the
  // 'domain' attribute when using setCookieString.
  testTrailingDotCookie("http://localhost", "localhost");
  testTrailingDotCookie("http://foo.com", "foo.com");

  cm.removeAll();
}

function getCookieCount() {
  var count = 0;
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
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
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);

  cm.removeAll();

  var uri = NetUtil.newURI(uriString);
  cs.setCookieString(uri, null, "foo=bar; domain=" + domain, null);
  var e = cm.getCookiesFromHost(domain, {});
  Assert.ok(e.hasMoreElements());
  Assert.equal(e.getNext().QueryInterface(Ci.nsICookie2).host, domain);
  cm.removeAll();

  cs.setCookieString(uri, null, "foo=bar; domain=." + domain, null);
  e = cm.getCookiesFromHost(domain, {});
  Assert.ok(e.hasMoreElements());
  Assert.equal(e.getNext().QueryInterface(Ci.nsICookie2).host, domain);
  cm.removeAll();
}

function testTrailingDotCookie(uriString, domain) {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);

  cm.removeAll();

  var uri = NetUtil.newURI(uriString);
  cs.setCookieString(uri, null, "foo=bar; domain=" + domain + ".", null);
  Assert.equal(cm.countCookiesFromHost(domain), 0);
  Assert.equal(cm.countCookiesFromHost(domain + "."), 0);
  cm.removeAll();

  uri = NetUtil.newURI(uriString + ".");
  cs.setCookieString(uri, null, "foo=bar; domain=" + domain, null);
  Assert.equal(cm.countCookiesFromHost(domain), 0);
  Assert.equal(cm.countCookiesFromHost(domain + "."), 0);
  cm.removeAll();
}

