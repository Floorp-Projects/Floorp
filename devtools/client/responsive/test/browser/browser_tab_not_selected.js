/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify RDM opens for the correct tab, even if it is not the currently
// selected tab.

const TEST_URL = "http://example.com/";

addRDMTask(
  null,
  async function() {
    info("Open two tabs");
    const tab1 = await addTab(TEST_URL);
    const tab2 = await addTab(TEST_URL);

    is(gBrowser.selectedTab, tab2, "The selected tab is tab2");

    info("Open RDM for the non-selected tab");
    const { ui } = await openRDM(tab1);
    await waitForRDMLoaded(ui, { waitForDeviceList: true });

    ok(!ResponsiveUIManager.isActiveForTab(tab2), "RDM is not opened on tab2");

    // Not mandatory for the test to pass, but it is helpful to see the RDM tab
    // for Try failure screenshots.
    info("Select the first tab");
    gBrowser.selectedTab = tab1;

    info("Try to update the DPI");
    await selectDevicePixelRatio(ui, 2);
    const dppx = await waitForDevicePixelRatio(ui, 2, {
      waitForTargetConfiguration: true,
    });
    is(dppx, 2, "Content has expected devicePixelRatio");

    const clientClosed = waitForClientClose(ui);
    await removeTab(tab2);
    await removeTab(tab1);
    await clientClosed;
  },
  { onlyPrefAndTask: true }
);
