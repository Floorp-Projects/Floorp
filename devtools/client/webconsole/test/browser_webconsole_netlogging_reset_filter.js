/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that network log messages bring up the network panel and select the
// right request even if it was previously filtered off.

"use strict";

const TEST_FILE_URI =
  "http://example.com/browser/devtools/client/webconsole/test/" +
  "test-network.html";
const TEST_URI = "data:text/html;charset=utf8,<p>test file URI";

var hud;

add_task(function* () {
  let Actions = require("devtools/client/netmonitor/actions/index");

  let requests = [];
  let { browser } = yield loadTab(TEST_URI);

  yield pushPrefEnv();
  hud = yield openConsole();
  hud.jsterm.clearOutput();

  HUDService.lastFinishedRequest.callback = request => requests.push(request);

  let loaded = loadBrowser(browser);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_FILE_URI);
  yield loaded;

  yield testMessages();
  let htmlRequest = requests.find(e => e.request.url.endsWith("html"));
  ok(htmlRequest, "htmlRequest was a html");

  yield hud.ui.openNetworkPanel(htmlRequest.actor);
  let toolbox = gDevTools.getToolbox(hud.target);
  is(toolbox.currentToolId, "netmonitor", "Network panel was opened");

  let panel = toolbox.getCurrentPanel();
  let selected = panel.panelWin.NetMonitorView.RequestsMenu.selectedItem;
  is(selected.attachment.method, htmlRequest.request.method,
     "The correct request is selected");
  is(selected.attachment.url, htmlRequest.request.url,
     "The correct request is definitely selected");

  // Filter out the HTML request.
  panel.panelWin.gStore.dispatch(Actions.toggleFilterType("js"));

  yield toolbox.selectTool("webconsole");
  is(toolbox.currentToolId, "webconsole", "Web console was selected");
  yield hud.ui.openNetworkPanel(htmlRequest.actor);

  panel.panelWin.NetMonitorView.RequestsMenu.selectedItem;
  is(selected.attachment.method, htmlRequest.request.method,
     "The correct request is selected");
  is(selected.attachment.url, htmlRequest.request.url,
     "The correct request is definitely selected");

  // All tests are done. Shutdown.
  HUDService.lastFinishedRequest.callback = null;
  htmlRequest = browser = requests = hud = null;
});

function testMessages() {
  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "running network console logging tests",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    },
    {
      text: "test-network.html",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    },
    {
      text: "testscript.js",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  });
}

function pushPrefEnv() {
  let deferred = promise.defer();
  let options = {
    set: [["devtools.webconsole.filter.networkinfo", true]]
  };
  SpecialPowers.pushPrefEnv(options, deferred.resolve);
  return deferred.promise;
}
