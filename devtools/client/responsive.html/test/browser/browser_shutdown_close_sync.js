/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify RDM closes synchronously when tabs are closed.

const TEST_URL = "http://example.com/";

function waitForClientClose(ui) {
  return new Promise(resolve => {
    info("RDM's debugger client is now closed");
    ui.client.addOneTimeListener("closed", resolve);
  });
}

add_task(function* () {
  let tab = yield addTab(TEST_URL);

  let { ui } = yield openRDM(tab);
  let clientClosed = waitForClientClose(ui);

  closeRDM(tab, {
    reason: "TabClose",
  });

  // This flag is set at the end of `ResponsiveUI.destroy`.  If it is true
  // without yielding on `closeRDM` above, then we must have closed
  // synchronously.
  is(ui.destroyed, true, "RDM closed synchronously");

  yield clientClosed;
  yield removeTab(tab);
});

add_task(function* () {
  let tab = yield addTab(TEST_URL);

  let { ui } = yield openRDM(tab);
  let clientClosed = waitForClientClose(ui);

  yield removeTab(tab);

  // This flag is set at the end of `ResponsiveUI.destroy`.  If it is true without
  // yielding on `closeRDM` itself and only removing the tab, then we must have closed
  // synchronously in response to tab closing.
  is(ui.destroyed, true, "RDM closed synchronously");

  yield clientClosed;
});
