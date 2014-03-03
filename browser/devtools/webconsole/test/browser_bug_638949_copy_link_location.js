/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
  "test/test-console.html?_date=" + Date.now();
const COMMAND_NAME = "consoleCmd_copyURL";
const CONTEXT_MENU_ID = "#menu_copyURL";

let HUD = null;
let output = null;
let menu = null;

function test() {
  let originalNetPref = Services.prefs.getBoolPref("devtools.webconsole.filter.networkinfo");
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("devtools.webconsole.filter.networkinfo", originalNetPref);
    HUD = output = menu = null;
  });

  Services.prefs.setBoolPref("devtools.webconsole.filter.networkinfo", true);

  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    openConsole(null, function (aHud) {
      HUD = aHud;
      output = aHud.outputNode;
      menu = HUD.iframeWindow.document.getElementById("output-contextmenu");

      executeSoon(testWithoutNetActivity);
    });
  }, true);
}

// Return whether "Copy Link Location" command is enabled or not.
function isEnabled() {
  let controller = top.document.commandDispatcher
                   .getControllerForCommand(COMMAND_NAME);
  return controller && controller.isCommandEnabled(COMMAND_NAME);
}

function testWithoutNetActivity() {
  HUD.jsterm.clearOutput();
  content.console.log("bug 638949");

  // Test that the "Copy Link Location" command is disabled for non-network
  // messages.
  waitForMessages({
    webconsole: HUD,
    messages: [{
      text: "bug 638949",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(onConsoleMessage);
}

function onConsoleMessage(aResults) {
  output.focus();
  let message = [...aResults[0].matched][0];

  goUpdateCommand(COMMAND_NAME);
  ok(!isEnabled(), COMMAND_NAME + "is disabled");

  // Test that the "Copy Link Location" menu item is hidden for non-network
  // messages.
  waitForContextMenu(menu, message, () => {
    let isHidden = menu.querySelector(CONTEXT_MENU_ID).hidden;
    ok(isHidden, CONTEXT_MENU_ID + " is hidden");
  }, testWithNetActivity);
}

function testWithNetActivity() {
  HUD.jsterm.clearOutput();
  content.location.reload(); // Reloading will produce network logging

  // Test that the "Copy Link Location" command is enabled and works
  // as expected for any network-related message.
  // This command should copy only the URL.
  waitForMessages({
    webconsole: HUD,
    messages: [{
      text: "test-console.html",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  }).then(onNetworkMessage);
}

function onNetworkMessage(aResults) {
  output.focus();
  let message = [...aResults[0].matched][0];
  HUD.ui.output.selectMessage(message);

  goUpdateCommand(COMMAND_NAME);
  ok(isEnabled(), COMMAND_NAME + " is enabled");

  info("expected clipboard value: " + message.url);

  waitForClipboard((aData) => { return aData.trim() == message.url; },
    () => { goDoCommand(COMMAND_NAME) },
    testMenuWithNetActivity, testMenuWithNetActivity);

  function testMenuWithNetActivity() {
    // Test that the "Copy Link Location" menu item is visible for network-related
    // messages.
    waitForContextMenu(menu, message, () => {
      let isVisible = !menu.querySelector(CONTEXT_MENU_ID).hidden;
      ok(isVisible, CONTEXT_MENU_ID + " is visible");
    }, finishTest);
  }
}

