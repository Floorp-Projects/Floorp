var gTestTab;

function test() {
  waitForExplicitFinish();

  is(getTopWin(), window, "got top window");
  is(getBoolPref("browser.search.openintab", false), false, "getBoolPref");
  is(getBoolPref("this.pref.doesnt.exist", true), true, "getBoolPref fallback");
  is(getBoolPref("this.pref.doesnt.exist", false), false, "getBoolPref fallback #2");


  gTestTab = openNewTabWith("http://example.com");
  gBrowser.selectedTab = gTestTab;
  gTestTab.linkedBrowser.addEventListener("load", function () {
    gTestTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    is(gTestTab.linkedBrowser.currentURI.spec, "http://example.com/", "example.com loaded");

    test_openUILink();
  }, true);
}

function test_openUILink() {
  gTestTab.linkedBrowser.addEventListener("load", function () {
    gTestTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    is(gTestTab.linkedBrowser.currentURI.spec, "http://example.org/", "example.org loaded");

    gBrowser.removeTab(gTestTab);
    finish();
  }, true);

  //openUILink(url, e, ignoreButton, ignoreAlt, allowKeywordFixup, postData, referrerUrl);
  openUILink("http://example.org"); // defaults to "current"
}
