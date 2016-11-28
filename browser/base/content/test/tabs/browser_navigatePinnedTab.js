/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function* () {
  // Test that changing the URL in a pinned tab works correctly

  let TEST_LINK_INITIAL = "about:";
  let TEST_LINK_CHANGED = "about:support";

  let appTab = gBrowser.addTab(TEST_LINK_INITIAL);
  let browser = appTab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);

  gBrowser.pinTab(appTab);
  is(appTab.pinned, true, "Tab was successfully pinned");

  let initialTabsNo = gBrowser.tabs.length;

  let goButton = document.getElementById("urlbar-go-button");
  gBrowser.selectedTab = appTab;
  gURLBar.focus();
  gURLBar.value = TEST_LINK_CHANGED;

  goButton.click();
  yield BrowserTestUtils.browserLoaded(browser);

  is(appTab.linkedBrowser.currentURI.spec, TEST_LINK_CHANGED,
     "New page loaded in the app tab");
  is(gBrowser.tabs.length, initialTabsNo, "No additional tabs were opened");

  // Now check that opening a link that does create a new tab works,
  // and also that it nulls out the opener.
  let pageLoadPromise = BrowserTestUtils.browserLoaded(appTab.linkedBrowser, "http://example.com/");
  yield BrowserTestUtils.loadURI(appTab.linkedBrowser, "http://example.com/");
  info("Started loading example.com");
  yield pageLoadPromise;
  info("Loaded example.com");
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://example.org/");
  yield ContentTask.spawn(browser, null, function* () {
    let link = content.document.createElement("a");
    link.href = "http://example.org/";
    content.document.body.appendChild(link);
    link.click();
  });
  info("Created & clicked link");
  let extraTab = yield newTabPromise;
  info("Got a new tab");
  yield ContentTask.spawn(extraTab.linkedBrowser, null, function* () {
    is(content.opener, null, "No opener should be available");
  });
  yield BrowserTestUtils.removeTab(extraTab);
});


registerCleanupFunction(function() {
  gBrowser.removeTab(gBrowser.selectedTab);
});
