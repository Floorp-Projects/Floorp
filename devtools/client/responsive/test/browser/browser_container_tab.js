/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify RDM opens for a container tab.

const TEST_URL = "http://example.com/";

addRDMTask(
  null,
  async function() {
    // Open a tab with about:newtab in a container.
    const tab = await addTab(BROWSER_NEW_TAB_URL, {
      userContextId: 2,
    });
    is(tab.userContextId, 2, "Tab's container ID is correct");

    // Open RDM and try to navigate
    const { ui } = await openRDM(tab);
    await waitForDeviceAndViewportState(ui);

    await navigateToNewDomain(TEST_URL, ui);
    ok(true, "Test URL navigated successfully");

    await closeRDM(tab);
    await removeTab(tab);
  },
  { usingBrowserUI: true, onlyPrefAndTask: true }
);
