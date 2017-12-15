/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/";
const TEST_URI = TEST_PATH + TEST_FILE;

add_task(async function task() {
  await pushPref("devtools.webconsole.filter.net", false);
  await pushPref("devtools.webconsole.filter.netxhr", true);
  await openNewTabAndToolbox(TEST_URI, "netmonitor");

  const currentTab = gBrowser.selectedTab;
  let target = TargetFactory.forTab(currentTab);
  let toolbox = gDevTools.getToolbox(target);

  let monitor = toolbox.getCurrentPanel();
  let netReady = monitor.panelWin.once("NetMonitor:PayloadReady");

  // Fire an XHR POST request.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function () {
    content.wrappedJSObject.testXhrGet();
  });

  info("XHR executed");

  await netReady;

  info("NetMonitor:PayloadReady received");

  let webconsolePanel = await toolbox.selectTool("webconsole");
  let { hud } = webconsolePanel;

  let xhrUrl = TEST_PATH + "test-data.json";
  let messageNode = await waitFor(() => findMessage(hud, xhrUrl));
  let urlNode = messageNode.querySelector(".url");
  info("Network message found.");

  let ui = hud.ui;
  let consoleReady = ui.jsterm.hud.once("network-request-payload-ready");

  // Expand network log
  urlNode.click();

  await consoleReady;

  info("network-request-payload-ready received");
  await testNetworkMessage(messageNode);
  await waitForLazyRequests(toolbox);
});

async function testNetworkMessage(messageNode) {
  let headersTab = messageNode.querySelector("#headers-tab");

  ok(headersTab, "Headers tab is available");

  // Headers tab should be selected by default, so just check its content.
  let headersContent;
  await waitUntil(() => {
    headersContent = messageNode.querySelector(
      "#headers-panel .headers-overview");
    return headersContent;
  });

  ok(headersContent, "Headers content is available");
}

/**
 * Wait until all lazily fetch requests in netmonitor get finsished.
 * Otherwise test will be shutdown too early and cause failure.
 */
async function waitForLazyRequests(toolbox) {
  let { ui } = toolbox.getCurrentPanel().hud;
  let proxy = ui.jsterm.hud.proxy;
  return waitUntil(() => {
    return !proxy.networkDataProvider.lazyRequestData.size;
  });
}
