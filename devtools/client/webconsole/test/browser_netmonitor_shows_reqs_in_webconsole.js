/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/netmonitor/test/shared-head.js", this);

const TEST_URI = "data:text/html;charset=utf8,Test that the netmonitor " +
                 "displays requests that have been recorded in the " +
                 "web console, even if the netmonitor hadn't opened yet.";

const TEST_FILE = "test-network-request.html";
const TEST_PATH = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/" + TEST_FILE;

const NET_PREF = "devtools.webconsole.filter.networkinfo";
Services.prefs.setBoolPref(NET_PREF, true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(NET_PREF);
});

add_task(function* () {
  let { tab, browser } = yield loadTab(TEST_URI);

  // Test that the request appears in the console.
  let hud = yield openConsole();
  info("Web console is open");

  yield loadDocument(browser);
  info("Document loaded.");

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "network message",
        text: TEST_FILE,
        category: CATEGORY_NETWORK,
        severity: SEVERITY_LOG
      }
    ]
  });

  // Test that the request appears in the network panel.
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "netmonitor");
  info("Network panel is open.");

  yield testNetmonitor(toolbox);
});

function loadDocument(browser) {
  let deferred = defer();

  browser.addEventListener("load", function () {
    deferred.resolve();
  }, {capture: true, once: true});
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_PATH);

  return deferred.promise;
}

function* testNetmonitor(toolbox) {
  let monitor = toolbox.getCurrentPanel();

  let { store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { getSortedRequests } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  yield waitUntil(() => store.getState().requests.requests.size > 0);

  is(store.getState().requests.requests.size, 1, "Network request appears in the network panel");

  let item = getSortedRequests(store.getState()).get(0);
  is(item.method, "GET", "The attached method is correct.");
  is(item.url, TEST_PATH, "The attached url is correct.");

  yield waitForExistingRequests(monitor);
}
