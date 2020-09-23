/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,Test that 'Open in Network Panel' " +
  "context menu item opens the selected request in netmonitor panel.";

const TEST_FILE = "test-network-request.html";
const JSON_TEST_URL = "test-network-request.html";
const TEST_PATH =
  "http://example.com/browser/devtools/client/webconsole/test/browser/";

const NET_PREF = "devtools.webconsole.filter.net";
const XHR_PREF = "devtools.webconsole.filter.netxhr";

Services.prefs.setBoolPref(NET_PREF, true);
Services.prefs.setBoolPref(XHR_PREF, true);

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref(NET_PREF);
  Services.prefs.clearUserPref(XHR_PREF);

  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

add_task(async function task() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const currentTab = gBrowser.selectedTab;
  const target = await TargetFactory.forTab(currentTab);
  const toolbox = gDevTools.getToolbox(target);

  const documentUrl = TEST_PATH + TEST_FILE;
  await navigateTo(documentUrl);
  info("Document loaded.");

  await openMessageInNetmonitor(toolbox, hud, documentUrl);

  info(
    "Wait for the netmonitor headers panel to appear as it spawn RDP requests"
  );
  const netmonitor = toolbox.getCurrentPanel();
  await waitUntil(() =>
    netmonitor.panelWin.document.querySelector(
      "#headers-panel .headers-overview"
    )
  );

  info(
    "Wait for the event timings request which do not necessarily update the UI as timings may be undefined for cached requests"
  );
  await waitForRequestData(netmonitor.panelWin.store, ["eventTimings"], 0);

  // Go back to console.
  await toolbox.selectTool("webconsole");
  info("console panel open again.");

  // Fire an XHR request.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    // Ensure XHR request is completed
    await new Promise(resolve => content.wrappedJSObject.testXhrGet(resolve));
  });

  const jsonUrl = TEST_PATH + JSON_TEST_URL;
  await openMessageInNetmonitor(toolbox, hud, jsonUrl);

  info(
    "Wait for the netmonitor headers panel to appear as it spawn RDP requests"
  );
  await waitUntil(() =>
    netmonitor.panelWin.document.querySelector(
      "#headers-panel .headers-overview"
    )
  );

  info(
    "Wait for the event timings request which do not necessarily update the UI as timings may be undefined for cached requests"
  );

  // Hide the header panel to get the eventTimings
  const { windowRequire } = netmonitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  info("Closing the header panel");
  await netmonitor.panelWin.store.dispatch(Actions.toggleNetworkDetails());

  await waitForRequestData(netmonitor.panelWin.store, ["eventTimings"], 1);
});

const {
  getSortedRequests,
} = require("devtools/client/netmonitor/src/selectors/index");

function waitForRequestData(store, fields, i) {
  return waitUntil(() => {
    const item = getSortedRequests(store.getState())[i];
    if (!item) {
      return false;
    }
    for (const field of fields) {
      if (!item[field]) {
        return false;
      }
    }
    return true;
  });
}
