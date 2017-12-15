/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Test that 'Open in Network Panel' " +
                 "context menu item opens the selected request in netmonitor panel.";

const TEST_FILE = "test-network-request.html";
const JSON_TEST_URL = "test-network-request.html";
const TEST_PATH = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/";

const NET_PREF = "devtools.webconsole.filter.net";
const XHR_PREF = "devtools.webconsole.filter.netxhr";

Services.prefs.setBoolPref(NET_PREF, true);
Services.prefs.setBoolPref(XHR_PREF, true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(NET_PREF);
  Services.prefs.clearUserPref(XHR_PREF);
});

add_task(async function task() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const currentTab = gBrowser.selectedTab;
  let target = TargetFactory.forTab(currentTab);
  let toolbox = gDevTools.getToolbox(target);

  const documentUrl = TEST_PATH + TEST_FILE;
  await loadDocument(documentUrl);
  info("Document loaded.");

  await testNetmonitorLink(toolbox, hud, documentUrl);

  // Go back to console.
  await toolbox.selectTool("webconsole");
  info("console panel open again.");

  // Fire an XHR request.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function () {
    content.wrappedJSObject.testXhrGet();
  });

  const jsonUrl = TEST_PATH + JSON_TEST_URL;
  await testNetmonitorLink(toolbox, hud, jsonUrl);
});

async function testNetmonitorLink(toolbox, hud, url) {
  let messageNode = await waitFor(() => findMessage(hud, url));
  info("Network message found.");

  let onNetmonitorSelected = toolbox.once("netmonitor-selected", (event, panel) => {
    return panel;
  });

  let menuPopup = await openContextMenu(hud, messageNode);
  let openInNetMenuItem = menuPopup.querySelector("#console-menu-open-in-network-panel");
  ok(openInNetMenuItem, "open in network panel item is enabled");
  openInNetMenuItem.click();

  const {panelWin} = await onNetmonitorSelected;
  ok(true, "The netmonitor panel is selected when clicking on the network message");

  let { store, windowRequire } = panelWin;
  let actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { getSelectedRequest } =
    windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(actions.batchEnable(false));

  await waitUntil(() => {
    const selected = getSelectedRequest(store.getState());
    return selected && selected.url === url;
  });

  ok(true, "The attached url is correct.");
}
