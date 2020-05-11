/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that reload conditions restore old-RDM's tab to the proper state.
// Bug 1585097: This test will be deleted once browser UI-enabled RDM is shipped to stable.

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(
  null,
  async function() {
    info("Ensure reload conditions for touch simulation are enabled.");
    reloadOnTouchChange(true);

    const tab = await addTab(TEST_URL);
    const { ui } = await openRDM(tab);
    await toggleTouchSimulation(ui);

    info("Wait for the UI's target to reload when opening RDM.");
    await once(ui.currentTarget, "navigate");
    ok(true, "RDM's tab target reloaded");

    info("Wait for the tab to reload when closing RDM.");
    const reloaded = BrowserTestUtils.waitForContentEvent(
      tab.linkedBrowser,
      "load",
      true
    );
    await closeRDM(tab);
    await reloaded;
    ok(true, "Tab reloaded");

    reloadOnTouchChange(false);
  },
  { usingBrowserUI: false, onlyPrefAndTask: true }
);
