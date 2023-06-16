/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify RDM closes synchronously when tabs are closed.

const TEST_URL = "http://example.com/";

addRDMTask(
  null,
  async function () {
    const tab = await addTab(TEST_URL);

    const { ui } = await openRDM(tab);
    await waitForDeviceAndViewportState(ui);
    const clientClosed = waitForClientClose(ui);

    closeRDM(tab, {
      reason: "TabClose",
    });

    // This flag is set at the end of `ResponsiveUI.destroy`.  If it is true
    // without waiting for `closeRDM` above, then we must have closed
    // synchronously.
    is(ui.destroyed, true, "RDM closed synchronously");

    await clientClosed;
    await removeTab(tab);
  },
  { onlyPrefAndTask: true }
);

addRDMTask(
  null,
  async function () {
    const tab = await addTab(TEST_URL);

    const { ui } = await openRDM(tab);
    await waitForDeviceAndViewportState(ui);
    const clientClosed = waitForClientClose(ui);

    await removeTab(tab);

    // This flag is set at the end of `ResponsiveUI.destroy`.  If it is true without
    // waiting for `closeRDM` itself and only removing the tab, then we must have closed
    // synchronously in response to tab closing.
    is(ui.destroyed, true, "RDM closed synchronously");

    await clientClosed;
  },
  { onlyPrefAndTask: true }
);
