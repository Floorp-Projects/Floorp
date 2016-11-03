/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Set ourselves up for TLS error
Services.prefs.setIntPref("security.tls.version.max", 3);
Services.prefs.setIntPref("security.tls.version.min", 3);

const LOW_TLS_VERSION = "https://tls1.example.com/";
const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});
const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

add_task(function* checkReturnToPreviousPage() {
  info("Loading a TLS page that isn't supported, ensure we have a fix button and clicking it then loads the page");
  let browser;
  let pageLoaded;
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = gBrowser.addTab(LOW_TLS_VERSION);
    browser = gBrowser.selectedBrowser;
    pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
  }, false);

  info("Loading and waiting for the net error");
  yield pageLoaded;

  Assert.ok(content.document.getElementById("prefResetButton").getBoundingClientRect().left >= 0,
    "Should have a visible button");

  Assert.ok(content.document.documentURI.startsWith("about:neterror"), "Should be showing error page");

  let pageshowPromise = promiseWaitForEvent(browser, "pageshow");
  yield ContentTask.spawn(browser, null, function* () {
    content.document.getElementById("prefResetButton").click();
  });
  yield pageshowPromise;

  Assert.equal(content.document.documentURI, LOW_TLS_VERSION, "Should not be showing page");

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
