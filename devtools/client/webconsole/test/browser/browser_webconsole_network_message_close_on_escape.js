/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "https://example.com/browser/devtools/client/webconsole/test/browser/";
const TEST_URI = TEST_PATH + TEST_FILE;

add_task(async function task() {
  await pushPref("devtools.webconsole.filter.netxhr", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  const currentTab = gBrowser.selectedTab;
  const target = await TargetFactory.forTab(currentTab);
  const toolbox = gDevTools.getToolbox(target);

  const xhrUrl = TEST_PATH + "test-data.json";
  const onMessage = waitForMessage(hud, xhrUrl);

  // Fire an XHR POST request.
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.testXhrGet();
  });

  const { node: messageNode } = await onMessage;
  ok(messageNode, "Network message found.");

  // Expand network log
  info("Click on XHR message to display network detail panel");
  messageNode.querySelector(".url").click();
  const headersTab = await waitFor(() =>
    messageNode.querySelector("#headers-tab")
  );
  ok(headersTab, "Headers tab is available");

  // Wait for all network RDP request to be finished and have updated the UI
  await waitUntil(() =>
    messageNode.querySelector("#headers-panel .headers-overview")
  );

  info("Focus header tab and hit Escape");
  headersTab.focus();
  EventUtils.sendKey("ESCAPE", toolbox.win);

  await waitFor(() => !messageNode.querySelector(".network-info"));
  ok(true, "The detail panel was closed on escape");
});
