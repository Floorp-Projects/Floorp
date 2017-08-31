/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Test that clicking on a network message " +
                 "in the console toggles the HTTP inspection.";

const TEST_FILE = "test-network-request.html";
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

  await testNetworkMessage(hud, documentUrl);
  await waitForNetworkUpdates(toolbox);
});

async function testNetworkMessage(hud, url) {
  let messageNode = await waitFor(() => findMessage(hud, url));
  let urlNode = messageNode.querySelector(".url");
  info("Network message found.");

  EventUtils.sendMouseEvent({ type: "click" }, urlNode);

  let headersTab = messageNode.querySelector("#headers-tab");
  let cookiesTab = messageNode.querySelector("#cookies-tab");
  let paramsTab = messageNode.querySelector("#params-tab");
  let responseTab = messageNode.querySelector("#response-tab");
  let timingsTab = messageNode.querySelector("#timings-tab");

  ok(headersTab, "Headers tab is available");
  ok(cookiesTab, "Cookies tab is available");
  ok(paramsTab, "Params tab is available");
  ok(responseTab, "Response tab is available");
  ok(timingsTab, "Timings tab is available");
}

async function waitForNetworkUpdates(toolbox) {
  let panel = toolbox.getCurrentPanel();
  let hud = panel.hud;
  let ui = hud.ui;

  return new Promise(resolve => {
    ui.jsterm.hud.on("network-request-payload-ready", () => {
      resolve();
    });
  });
}
