/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a test for the Open URL context menu item
// that is shown for network requests

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";
const COMMAND_NAME = "consoleCmd_openURL";
const CONTEXT_MENU_ID = "#menu_openURL";

var HUD = null, outputNode = null, contextMenu = null;

var test = asyncTest(function* () {
  Services.prefs.setBoolPref("devtools.webconsole.filter.networkinfo", true);

  yield loadTab(TEST_URI);
  HUD = yield openConsole();

  let results = yield consoleOpened();
  yield onConsoleMessage(results);

  let results2 = yield testOnNetActivity();
  let msg = yield onNetworkMessage(results2);

  yield testOnNetActivityContextMenu(msg);

  Services.prefs.clearUserPref("devtools.webconsole.filter.networkinfo");

  HUD = null;
  outputNode = null;
  contextMenu = null;
});

function consoleOpened() {
  outputNode = HUD.outputNode;
  contextMenu = HUD.iframeWindow.document.getElementById("output-contextmenu");

  HUD.jsterm.clearOutput();

  content.console.log("bug 764572");

  return waitForMessages({
    webconsole: HUD,
    messages: [{
      text: "bug 764572",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
}

function onConsoleMessage(results) {
  outputNode.focus();
  outputNode.selectedItem = [...results[0].matched][0];

  // Check if the command is disabled non-network messages.
  goUpdateCommand(COMMAND_NAME);
  let controller = top.document.commandDispatcher
                   .getControllerForCommand(COMMAND_NAME);

  let isDisabled = !controller || !controller.isCommandEnabled(COMMAND_NAME);
  ok(isDisabled, COMMAND_NAME + " should be disabled.");

  outputNode.selectedItem.scrollIntoView();
  return waitForContextMenu(contextMenu, outputNode.selectedItem, () => {
    let isHidden = contextMenu.querySelector(CONTEXT_MENU_ID).hidden;
    ok(isHidden, CONTEXT_MENU_ID + " should be hidden.");
  });
}

function testOnNetActivity() {
  HUD.jsterm.clearOutput();

  // Reload the url to show net activity in console.
  content.location.reload();

  return waitForMessages({
    webconsole: HUD,
    messages: [{
      text: "test-console.html",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  });
}

function onNetworkMessage(results) {
  let deferred = promise.defer();

  outputNode.focus();
  let msg = [...results[0].matched][0];
  ok(msg, "network message");
  HUD.ui.output.selectMessage(msg);

  let currentTab = gBrowser.selectedTab;
  let newTab = null;

  gBrowser.tabContainer.addEventListener("TabOpen", function onOpen(evt) {
    gBrowser.tabContainer.removeEventListener("TabOpen", onOpen, true);
    newTab = evt.target;
    newTab.linkedBrowser.addEventListener("load", onTabLoaded, true);
  }, true);

  function onTabLoaded() {
    newTab.linkedBrowser.removeEventListener("load", onTabLoaded, true);
    gBrowser.removeTab(newTab);
    gBrowser.selectedTab = currentTab;
    executeSoon(deferred.resolve.bind(null, msg));
  }

  // Check if the command is enabled for a network message.
  goUpdateCommand(COMMAND_NAME);
  let controller = top.document.commandDispatcher
                   .getControllerForCommand(COMMAND_NAME);
  ok(controller.isCommandEnabled(COMMAND_NAME),
     COMMAND_NAME + " should be enabled.");

  // Try to open the URL.
  goDoCommand(COMMAND_NAME);

  return deferred.promise;
}

function testOnNetActivityContextMenu(msg) {
  let deferred = promise.defer();

  outputNode.focus();
  HUD.ui.output.selectMessage(msg);
  msg.scrollIntoView();

  info("net activity context menu");

  waitForContextMenu(contextMenu, msg, () => {
    let isShown = !contextMenu.querySelector(CONTEXT_MENU_ID).hidden;
    ok(isShown, CONTEXT_MENU_ID + " should be shown.");
  }).then(deferred.resolve);

  return deferred.promise;
}
