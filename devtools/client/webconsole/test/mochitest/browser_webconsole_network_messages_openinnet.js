/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Test that 'Open in Network Panel' " +
                 "context menu item opens the selected request in netmonitor panel.";

const TEST_FILE = "test-network-request.html";
const JSON_TEST_URL = "test-network-request.html";
const TEST_PATH = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/mochitest/";

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
  const target = TargetFactory.forTab(currentTab);
  const toolbox = gDevTools.getToolbox(target);

  const documentUrl = TEST_PATH + TEST_FILE;
  await loadDocument(documentUrl);
  info("Document loaded.");

  await openMessageInNetmonitor(toolbox, hud, documentUrl);

  // Go back to console.
  await toolbox.selectTool("webconsole");
  info("console panel open again.");

  // Fire an XHR request.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.testXhrGet();
  });

  const jsonUrl = TEST_PATH + JSON_TEST_URL;
  await openMessageInNetmonitor(toolbox, hud, jsonUrl);
});
