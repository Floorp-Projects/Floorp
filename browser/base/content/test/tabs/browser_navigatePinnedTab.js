/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  // Test that changing the URL in a pinned tab works correctly

  let TEST_LINK_INITIAL = "about:";
  let TEST_LINK_CHANGED = "about:support";

  let appTab = BrowserTestUtils.addTab(gBrowser, TEST_LINK_INITIAL);
  let browser = appTab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  gBrowser.pinTab(appTab);
  is(appTab.pinned, true, "Tab was successfully pinned");

  let initialTabsNo = gBrowser.tabs.length;

  gBrowser.selectedTab = appTab;
  gURLBar.focus();
  gURLBar.value = TEST_LINK_CHANGED;

  gURLBar.goButton.click();
  await BrowserTestUtils.browserLoaded(browser);

  is(appTab.linkedBrowser.currentURI.spec, TEST_LINK_CHANGED,
     "New page loaded in the app tab");
  is(gBrowser.tabs.length, initialTabsNo, "No additional tabs were opened");

  // Now check that opening a link that does create a new tab works,
  // and also that it nulls out the opener.
  let pageLoadPromise = BrowserTestUtils.browserLoaded(appTab.linkedBrowser, "http://example.com/");
  await BrowserTestUtils.loadURI(appTab.linkedBrowser, "http://example.com/");
  info("Started loading example.com");
  await pageLoadPromise;
  info("Loaded example.com");
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://example.org/");
  await ContentTask.spawn(browser, null, async function() {
    let link = content.document.createElement("a");
    link.href = "http://example.org/";
    content.document.body.appendChild(link);
    link.click();
  });
  info("Created & clicked link");
  let extraTab = await newTabPromise;
  info("Got a new tab");
  await ContentTask.spawn(extraTab.linkedBrowser, null, async function() {
    is(content.opener, null, "No opener should be available");
  });
  await BrowserTestUtils.removeTab(extraTab);
});


registerCleanupFunction(function() {
  gBrowser.removeTab(gBrowser.selectedTab);
});
