/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function test_browser_open_newtab_default_url() {
  BrowserOpenTab();
  const tab = gBrowser.selectedTab;

  if (tab.linkedBrowser.currentURI.spec !== window.BROWSER_NEW_TAB_URL) {
    // If about:newtab is not loaded immediately, wait for any location change.
    await BrowserTestUtils.waitForLocationChange(gBrowser);
  }

  is(tab.linkedBrowser.currentURI.spec, window.BROWSER_NEW_TAB_URL);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_browser_open_newtab_specific_url() {
  const url = "https://example.com";

  BrowserOpenTab({ url });
  const tab = gBrowser.selectedTab;

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  is(tab.linkedBrowser.currentURI.spec, "https://example.com/");

  BrowserTestUtils.removeTab(tab);
});
