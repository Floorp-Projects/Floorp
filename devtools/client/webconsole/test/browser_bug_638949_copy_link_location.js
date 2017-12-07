/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for the "Copy link location" context menu item shown when you right
// click network requests in the output.

"use strict";

add_task(function* () {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
    "test/test-console.html?_date=" + Date.now();
  const COMMAND_NAME = "consoleCmd_copyURL";
  const CONTEXT_MENU_ID = "#menu_copyURL";

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("devtools.webconsole.filter.networkinfo");
  });

  Services.prefs.setBoolPref("devtools.webconsole.filter.networkinfo", true);

  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  let output = hud.outputNode;
  let menu = hud.iframeWindow.document.getElementById("output-contextmenu");

  hud.jsterm.clearOutput();
  content.console.log("bug 638949");

  // Test that the "Copy Link Location" command is disabled for non-network
  // messages.
  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug 638949",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  output.focus();
  let message = [...result.matched][0];

  goUpdateCommand(COMMAND_NAME);
  ok(!isEnabled(), COMMAND_NAME + " is disabled");

  // Test that the "Copy Link Location" menu item is hidden for non-network
  // messages.
  yield waitForContextMenu(menu, message, () => {
    let isHidden = menu.querySelector(CONTEXT_MENU_ID).hidden;
    ok(isHidden, CONTEXT_MENU_ID + " is hidden");
  });

  hud.jsterm.clearOutput();
  // Reloading will produce network logging
  content.location.reload();

  // Test that the "Copy Link Location" command is enabled and works
  // as expected for any network-related message.
  // This command should copy only the URL.
  [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test-console.html",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  });

  output.focus();
  message = [...result.matched][0];
  hud.ui.output.selectMessage(message);

  goUpdateCommand(COMMAND_NAME);
  ok(isEnabled(), COMMAND_NAME + " is enabled");

  info("expected clipboard value: " + message.url);

  let deferred = defer();

  waitForClipboard((aData) => {
    return aData.trim() == message.url;
  }, () => {
    goDoCommand(COMMAND_NAME);
  }, () => {
    deferred.resolve(null);
  }, () => {
    deferred.reject(null);
  });

  yield deferred.promise;

  // Test that the "Copy Link Location" menu item is visible for network-related
  // messages.
  yield waitForContextMenu(menu, message, () => {
    let isVisible = !menu.querySelector(CONTEXT_MENU_ID).hidden;
    ok(isVisible, CONTEXT_MENU_ID + " is visible");
  });

  // Return whether "Copy Link Location" command is enabled or not.
  function isEnabled() {
    let controller = top.document.commandDispatcher
                     .getControllerForCommand(COMMAND_NAME);
    return controller && controller.isCommandEnabled(COMMAND_NAME);
  }
});
