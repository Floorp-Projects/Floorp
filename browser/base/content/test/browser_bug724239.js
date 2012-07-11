/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  BrowserOpenTab();

  let tab = gBrowser.selectedTab;
  let browser = tab.linkedBrowser;

  registerCleanupFunction(function () { gBrowser.removeTab(tab); });

  whenBrowserLoaded(browser, function () {
    browser.loadURI("http://example.com/");

    whenBrowserLoaded(browser, function () {
      ok(!gBrowser.canGoBack, "about:newtab wasn't added to the session history");
      finish();
    });
  });
}

function whenBrowserLoaded(aBrowser, aCallback) {
  aBrowser.addEventListener("load", function onLoad() {
    aBrowser.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}
