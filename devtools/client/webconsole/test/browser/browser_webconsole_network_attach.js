/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "https://example.com/browser/devtools/client/webconsole/" + "test/browser/";
const TEST_URI = TEST_PATH + TEST_FILE;

registerCleanupFunction(async function () {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

add_task(async function task() {
  await pushPref("devtools.webconsole.filter.net", false);
  await pushPref("devtools.webconsole.filter.netxhr", true);
  await openNewTabAndToolbox(TEST_URI, "netmonitor");

  const currentTab = gBrowser.selectedTab;
  const toolbox = gDevTools.getToolboxForTab(currentTab);
  const panel = toolbox.getCurrentPanel().panelWin;

  const netReady = panel.api.once("NetMonitor:PayloadReady");

  // Fire an XHR POST request.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.testXhrGet();
  });

  info("XHR executed");

  await netReady;

  info("NetMonitor:PayloadReady received");

  const { hud } = await toolbox.selectTool("webconsole");

  const xhrUrl = TEST_PATH + "test-data.json";
  const messageNode = await waitFor(() =>
    findMessageByType(hud, xhrUrl, ".network")
  );
  const urlNode = messageNode.querySelector(".url");
  info("Network message found.");

  const onReady = hud.ui.once("network-request-payload-ready");

  // Expand network log
  urlNode.click();

  await onReady;

  info("network-request-payload-ready received");

  await testNetworkMessage(messageNode);
  await waitForLazyRequests(toolbox);
});

async function testNetworkMessage(messageNode) {
  const headersTab = messageNode.querySelector("#headers-tab");

  ok(headersTab, "Headers tab is available");

  // Headers tab should be selected by default, so just check its content.
  await waitUntil(() =>
    messageNode.querySelector("#headers-panel .headers-overview")
  );
}
