/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify RDM opens for a preloaded about:newtab browser.

const TEST_URL = "http://example.com/";

addRDMTask(
  null,
  async function() {
    // Open a tab with about:newtab.
    // Don't wait for load because the page is preloaded.
    const tab = await addTab(BROWSER_NEW_TAB_URL, {
      waitForLoad: false,
    });
    const browser = tab.linkedBrowser;
    is(
      browser.getAttribute("preloadedState"),
      "consumed",
      "Got a preloaded browser for newtab"
    );

    // Open RDM and try to navigate
    const { ui } = await openRDM(tab);
    await waitForDeviceAndViewportState(ui);

    await load(browser, TEST_URL);
    ok(true, "Test URL navigated successfully");

    await closeRDM(tab);
    await removeTab(tab);
  },
  { usingBrowserUI: true, onlyPrefAndTask: true }
);
