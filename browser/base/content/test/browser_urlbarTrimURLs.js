/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function testVal(originalValue, targetValue) {
  gURLBar.value = originalValue;
  gURLBar.valueIsTyped = false;
  is(gURLBar.value, targetValue || originalValue, "url bar value set");
}

function test() {
  const prefname = "browser.urlbar.trimURLs";

  gBrowser.selectedTab = gBrowser.addTab();

  registerCleanupFunction(function () {
    gBrowser.removeCurrentTab();
    Services.prefs.clearUserPref(prefname);
    URLBarSetURI();
  });

  Services.prefs.setBoolPref(prefname, true);

  testVal("http://mozilla.org/", "mozilla.org");
  testVal("https://mozilla.org/", "https://mozilla.org");
  testVal("http://mözilla.org/", "mözilla.org");
  testVal("http://mozilla.imaginatory/", "mozilla.imaginatory");
  testVal("http://www.mozilla.org/", "www.mozilla.org");
  testVal("http://sub.mozilla.org/", "sub.mozilla.org");
  testVal("http://sub1.sub2.sub3.mozilla.org/", "sub1.sub2.sub3.mozilla.org");
  testVal("http://mozilla.org/file.ext", "mozilla.org/file.ext");
  testVal("http://mozilla.org/sub/", "mozilla.org/sub/");

  testVal("http://ftp.mozilla.org/", "http://ftp.mozilla.org");
  testVal("ftp://ftp.mozilla.org/", "ftp://ftp.mozilla.org");

  testVal("https://user:pass@mozilla.org/", "https://user:pass@mozilla.org");
  testVal("http://user:pass@mozilla.org/", "http://user:pass@mozilla.org");
  testVal("http://sub.mozilla.org:666/", "sub.mozilla.org:666");

  testVal("https://[fe80::222:19ff:fe11:8c76]/file.ext");
  testVal("http://[fe80::222:19ff:fe11:8c76]/", "[fe80::222:19ff:fe11:8c76]");
  testVal("https://user:pass@[fe80::222:19ff:fe11:8c76]:666/file.ext");
  testVal("http://user:pass@[fe80::222:19ff:fe11:8c76]:666/file.ext");

  testVal("mailto:admin@mozilla.org");
  testVal("gopher://mozilla.org/");
  testVal("about:config");
  testVal("jar:http://mozilla.org/example.jar!/");
  testVal("view-source:http://mozilla.org/");

  Services.prefs.setBoolPref(prefname, false);

  testVal("http://mozilla.org/");

  Services.prefs.setBoolPref(prefname, true);

  waitForExplicitFinish();

  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    is(gBrowser.currentURI.spec, "http://example.com/", "expected page should have loaded");

    testCopy("example.com", "http://example.com/", function () {
      SetPageProxyState("invalid");
      gURLBar.valueIsTyped = true;
      testCopy("example.com", "example.com", finish);
    });
  }, true);

  gBrowser.loadURI("http://example.com/");
}

function testCopy(originalValue, targetValue, cb) {
  waitForClipboard(targetValue, function () {
    is(gURLBar.value, originalValue, "url bar copy value set");

    gURLBar.focus();
    gURLBar.select();
    goDoCommand("cmd_copy");
  }, cb, cb);
}
