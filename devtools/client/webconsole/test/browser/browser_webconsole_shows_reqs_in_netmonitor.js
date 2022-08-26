/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>Test that the web console " +
  "displays requests that have been recorded in the " +
  "netmonitor, even if the console hadn't opened yet.";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "https://example.com/browser/devtools/client/webconsole/test/browser/" +
  TEST_FILE;

const NET_PREF = "devtools.webconsole.filter.net";
Services.prefs.setBoolPref(NET_PREF, true);
registerCleanupFunction(async () => {
  Services.prefs.clearUserPref(NET_PREF);

  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URI, "netmonitor");
  info("Network panel is open.");

  await navigateTo(TEST_PATH);
  info("Document loaded.");

  // Test that the request appears in the network panel.
  await testNetmonitor(toolbox);

  // Test that the request appears in the console.
  const { hud } = await toolbox.selectTool("webconsole");
  info("Web console is open");

  // We can't use `waitForMessages` here because the `new-messages` event
  // can be emitted before we get the `hud`.
  await waitFor(() => findMessageByType(hud, TEST_PATH, ".network"));

  ok(true, "The network message was found in the console");
});

async function testNetmonitor(toolbox) {
  const monitor = toolbox.getCurrentPanel();
  const { store, windowRequire } = monitor.panelWin;
  const { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  await waitFor(() => !!store.getState().requests.requests.length);

  is(
    store.getState().requests.requests.length,
    1,
    "Network request appears in the network panel"
  );

  const item = getSortedRequests(store.getState())[0];
  is(item.method, "GET", "The request method is correct.");
  is(item.url, TEST_PATH, "The request url is correct.");
}
