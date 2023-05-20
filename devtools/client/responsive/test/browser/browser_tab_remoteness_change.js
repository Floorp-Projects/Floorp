/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify Fission-enabled RDM remains open when tab changes remoteness.

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Permission denied to access property "document" on cross-origin object/
);

const Types = require("resource://devtools/client/responsive/types.js");

const TEST_URL = "http://example.com/";

addRDMTask(
  null,
  async function () {
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
    await navigateTo("about:robots");

    // Bug 1625501: RDM will remain open when the embedded browser UI is enabled.
    is(ui.destroyed, false, "RDM is still open.");

    info("Close RDM");
    await closeRDM(tab);

    await removeTab(tab);
  },
  { onlyPrefAndTask: true }
);
