/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "http://example.com/browser/devtools/client/webconsole/" + "test/browser/";
const TEST_URI = TEST_PATH + TEST_FILE;

add_task(async function task() {
  await pushPref("devtools.webconsole.filter.net", false);
  await pushPref("devtools.webconsole.filter.netxhr", true);
  await openNewTabAndToolbox(TEST_URI, "netmonitor");

  const currentTab = gBrowser.selectedTab;
  const target = await TargetFactory.forTab(currentTab);
  const toolbox = gDevTools.getToolbox(target);

  const monitor = toolbox.getCurrentPanel();
  const netReady = monitor.panelWin.api.once("NetMonitor:PayloadReady");

  // Fire an XHR POST request.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.testXhrGet();
  });

  info("XHR executed");

  await netReady;

  info("NetMonitor:PayloadReady received");

  const { hud } = await toolbox.selectTool("webconsole");

  const xhrUrl = TEST_PATH + "test-data.json";
  const messageNode = await waitFor(() => findMessage(hud, xhrUrl));
  const urlNode = messageNode.querySelector(".url");
  info("Network message found.");

  const consoleReady = hud.ui.once("network-request-payload-ready");

  // Expand network log
  urlNode.click();

  await consoleReady;

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

/**
 * Wait until all lazily fetch requests in netmonitor get finished.
 * Otherwise test will be shutdown too early and cause failure.
 */
async function waitForLazyRequests(toolbox) {
  const { ui } = toolbox.getCurrentPanel().hud;
  const proxy = ui.proxy;
  return waitUntil(() => {
    return !proxy.networkDataProvider.lazyRequestData.size;
  });
}
