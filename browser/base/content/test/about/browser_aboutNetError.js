/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Set ourselves up for TLS error
Services.prefs.setIntPref("security.tls.version.max", 3);
Services.prefs.setIntPref("security.tls.version.min", 3);

const LOW_TLS_VERSION = "https://tls1.example.com/";
const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});
const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

add_task(async function checkReturnToPreviousPage() {
  info("Loading a TLS page that isn't supported, ensure we have a fix button and clicking it then loads the page");
  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, LOW_TLS_VERSION);
    browser = gBrowser.selectedBrowser;
    pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
  }, false);

  info("Loading and waiting for the net error");
  await pageLoaded;

  // NB: This code assumes that the error page and the test page load in the
  // same process. If this test starts to fail, it could be because they load
  // in different processes.
  await ContentTask.spawn(browser, LOW_TLS_VERSION, async function(LOW_TLS_VERSION_) {
    ok(content.document.getElementById("prefResetButton").getBoundingClientRect().left >= 0,
      "Should have a visible button");

    ok(content.document.documentURI.startsWith("about:neterror"), "Should be showing error page");

    let doc = content.document;
    let prefResetButton = doc.getElementById("prefResetButton");
    is(prefResetButton.getAttribute("autofocus"), "true", "prefResetButton has autofocus");
    prefResetButton.click();

    await ContentTaskUtils.waitForEvent(this, "pageshow", true);

    is(content.document.documentURI, LOW_TLS_VERSION_, "Should not be showing page");
  });

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
