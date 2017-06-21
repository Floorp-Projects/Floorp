/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Test that the web console " +
                 "displays requests that have been recorded in the " +
                 "netmonitor, even if the console hadn't opened yet.";

const TEST_FILE = "test-network-request.html";
const TEST_PATH = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/" + TEST_FILE;

const NET_PREF = "devtools.webconsole.filter.net";
Services.prefs.setBoolPref(NET_PREF, true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(NET_PREF);
});

add_task(async function () {
  const toolbox = await openNewTabAndToolbox(TEST_URI, "netmonitor");
  const currentTab = gBrowser.selectedTab;
  info("Network panel is open.");

  await loadDocument(currentTab.linkedBrowser, TEST_PATH);
  info("Document loaded.");

  // Test that the request appears in the network panel.
  await testNetmonitor(toolbox);

  // Test that the request appears in the console.
  const { hud } = await toolbox.selectTool("webconsole");
  info("Web console is open");

  // We can't use `waitForMessages` here because the `new-messages` event
  // can be emitted before we get the `hud`.
  await waitFor(() => findMessage(hud, TEST_PATH));

  ok(true, "The network message was found in the console");
});

async function testNetmonitor(toolbox) {
  let monitor = toolbox.getCurrentPanel();
  let { store, windowRequire } = monitor.panelWin;
  let {
    getSortedRequests
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  await waitUntil(() => store.getState().requests.requests.size > 0);

  is(store.getState().requests.requests.size, 1,
    "Network request appears in the network panel");

  let item = getSortedRequests(store.getState()).get(0);
  is(item.method, "GET", "The request method is correct.");
  is(item.url, TEST_PATH, "The request url is correct.");
}
