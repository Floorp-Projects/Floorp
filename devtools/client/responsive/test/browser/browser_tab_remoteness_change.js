/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify Fission-enabled RDM remains open when tab changes remoteness.

const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(
  /Permission denied to access property "document" on cross-origin object/
);

const Types = require("devtools/client/responsive/types");

const TEST_URL = "http://example.com/";

addRDMTask(
  null,
  async function() {
    const tab = await addTab(TEST_URL);

    const { ui } = await openRDM(tab);
    const { store } = ui.toolWindow;
    await waitUntilState(
      store,
      state =>
        state.viewports.length == 1 &&
        state.devices.listState == Types.loadableState.LOADED
    );
    const clientClosed = waitForClientClose(ui);

    closeRDM(tab, {
      reason: "BeforeTabRemotenessChange",
    });

    // This flag is set at the end of `ResponsiveUI.destroy`.  If it is true
    // without waiting for `closeRDM` above, then we must have closed
    // synchronously.
    is(ui.destroyed, true, "RDM closed synchronously");

    await clientClosed;
    await removeTab(tab);
  },
  { usingBrowserUI: true, onlyPrefAndTask: true }
);

addRDMTask(
  null,
  async function() {
    const tab = await addTab(TEST_URL);

    const { ui } = await openRDM(tab);
    const { store } = ui.toolWindow;
    await waitUntilState(
      store,
      state =>
        state.viewports.length == 1 &&
        state.devices.listState == Types.loadableState.LOADED
    );

    // Load URL that requires the main process, forcing a remoteness flip
    await navigateToNewDomain("about:robots", ui);

    // Bug 1625501: RDM will remain open when the embedded browser UI is enabled.
    is(ui.destroyed, false, "RDM is still open.");

    info("Close RDM");
    await closeRDM(tab);

    await removeTab(tab);
  },
  { usingBrowserUI: true, onlyPrefAndTask: true }
);

// Remove this test when the new RDM is enabled in Firefox stable release. In the
// meantime, we should still test that old-RDM will close if needed when there is
// remoteness change in the tab.
addRDMTask(
  null,
  async function() {
    const tab = await addTab(TEST_URL);

    const { ui } = await openRDM(tab);
    const { store } = ui.toolWindow;
    await waitUntilState(
      store,
      state =>
        state.viewports.length == 1 &&
        state.devices.listState == Types.loadableState.LOADED
    );
    const clientClosed = waitForClientClose(ui);

    // Load URL that requires the main process, forcing a remoteness flip
    await load(tab.linkedBrowser, "about:robots");

    // RDM will close if using the old version.
    is(ui.destroyed, true, "RDM closed synchronously");

    await clientClosed;
    await removeTab(tab);
  },
  { usingBrowserUI: false, onlyPrefAndTask: true }
);
