/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the Copy Link Location menu item of the webconsole is displayed for network
// messages and copies the expected URL.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-console.html?_date=" +
                 Date.now();
const CONTEXT_MENU_ID = "#console-menu-copy-url";

add_task(async function() {
  // Enable net messages in the console for this test.
  await pushPref("devtools.webconsole.filter.net", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();

  info("Test Copy URL menu item for text log");

  info("Logging a text message in the content window");
  const onLogMessage = waitForMessage(hud, "simple text message");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.console.log("simple text message");
  });
  let message = await onLogMessage;
  ok(message, "Text log found in the console");

  info("Open and check the context menu for the logged text message");
  let menuPopup = await openContextMenu(hud, message.node);
  let copyURLItem = menuPopup.querySelector(CONTEXT_MENU_ID);
  ok(!copyURLItem, "Copy URL menu item is hidden for a simple text message");

  await hideContextMenu(hud);
  hud.ui.clearOutput();

  info("Test Copy URL menu item for network log");

  info("Reload the content window to produce a network log");
  const onNetworkMessage = waitForMessage(hud, "test-console.html");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.location.reload();
  });

  message = await onNetworkMessage;
  ok(message, "Network log found in the console");

  info("Open and check the context menu for the logged network message");
  menuPopup = await openContextMenu(hud, message.node);
  copyURLItem = menuPopup.querySelector(CONTEXT_MENU_ID);
  ok(copyURLItem, "Copy url menu item is available in context menu");

  info("Click on Copy URL menu item and wait for clipboard to be updated");
  await waitForClipboardPromise(() => copyURLItem.click(), TEST_URI);
  ok(true, "Expected text was copied to the clipboard.");

  await hideContextMenu(hud);
});
