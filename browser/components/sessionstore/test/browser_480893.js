"use strict";

/**
 * Tests that we get sent to the right page when the user clicks
 * the "Close" button in about:sessionrestore
 */
add_task(function*() {
  yield SpecialPowers.pushPrefEnv({
    "set": [
      ["browser.startup.page", 0],
    ]
  });

  let tab = gBrowser.addTab("about:sessionrestore");
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser, false, "about:sessionrestore");

  let doc = browser.contentDocument;

  // Click on the "Close" button after about:sessionrestore is loaded.
  doc.getElementById("errorCancel").click();

  yield BrowserTestUtils.browserLoaded(browser, false, "about:blank");

  // Test that starting a new session loads the homepage (set to http://mochi.test:8888)
  // if Firefox is configured to display a homepage at startup (browser.startup.page = 1)
  let homepage = "http://mochi.test:8888/";
  yield SpecialPowers.pushPrefEnv({
    "set": [
      ["browser.startup.homepage", homepage],
      ["browser.startup.page", 1],
    ]
  });

  browser.loadURI("about:sessionrestore");
  yield BrowserTestUtils.browserLoaded(browser, false, "about:sessionrestore");
  doc = browser.contentDocument;

  // Click on the "Close" button after about:sessionrestore is loaded.
  doc.getElementById("errorCancel").click();
  yield BrowserTestUtils.browserLoaded(browser);

  is(browser.currentURI.spec, homepage, "loaded page is the homepage");

  yield BrowserTestUtils.removeTab(tab);
});
