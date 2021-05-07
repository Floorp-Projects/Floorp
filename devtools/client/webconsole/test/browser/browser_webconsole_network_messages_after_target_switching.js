/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that after a target switch, the network details (headers, cookies etc) are available
// when the request message is expanded.

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "http://example.com/browser/devtools/client/webconsole/test/browser/";

const TEST_URI = TEST_PATH + TEST_FILE;

pushPref("devtools.webconsole.filter.net", true);
pushPref("devtools.webconsole.filter.netxhr", true);

registerCleanupFunction(async function() {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

add_task(async function task() {
  info("Add an empty tab and open the console");
  const hud = await openNewTabAndConsole("");

  const onMessageAvailable = waitForMessage(hud, TEST_URI, ".network");
  info(`Navigate to ${TEST_URI}`);
  await navigateTo(TEST_URI);
  const { node } = await onMessageAvailable;

  info(`Click on ${TEST_FILE} request`);
  node.querySelector(".url").click();

  info("Wait for the network detail panel to be displayed");
  await waitFor(
    () => node.querySelector(".network-info"),
    "Wait for .network-info to be rendered"
  );

  // Test that headers information is showing
  await waitFor(
    () => node.querySelector("#headers-panel .headers-overview"),
    "Headers overview info is visible"
  );
});
