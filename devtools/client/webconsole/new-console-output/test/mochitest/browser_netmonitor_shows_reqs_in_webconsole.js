/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Test that the netmonitor " +
                 "displays requests that have been recorded in the " +
                 "web console, even if the netmonitor hadn't opened yet.";

const TEST_FILE = "test-network-request.html";
const TEST_PATH = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/" + TEST_FILE;

const NET_PREF = "devtools.webconsole.filter.net";
Services.prefs.setBoolPref(NET_PREF, true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(NET_PREF);
});

add_task(async function task() {
  // Test that the request appears in the console.
  const hud = await openNewTabAndConsole(TEST_URI);
  const currentTab = gBrowser.selectedTab;
  info("Web console is open");

  const onMessageAdded = waitForMessages({
    hud,
    messages: [{
      text: TEST_PATH,
    }]
  });

  await loadDocument(currentTab.linkedBrowser, TEST_PATH);
  info("Document loaded.");

  await onMessageAdded;
  info("Network message found.");

  // Test that the request appears in the network panel.
  let target = TargetFactory.forTab(currentTab);
  let toolbox = await gDevTools.showToolbox(target, "netmonitor");
  info("Network panel is open.");

  await testNetmonitor(toolbox);
});

async function testNetmonitor(toolbox) {
  let monitor = toolbox.getCurrentPanel();

  let { store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { getSortedRequests } =
    windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  await waitUntil(() => store.getState().requests.requests.size > 0);

  is(store.getState().requests.requests.size, 1,
    "Network request appears in the network panel");

  let item = getSortedRequests(store.getState()).get(0);
  is(item.method, "GET", "The attached method is correct.");
  is(item.url, TEST_PATH, "The attached url is correct.");
}
