/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that we display the expected context menu entries.

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function() {
  await pushPref("devtools.browserconsole.contentMessages", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");
  // Enable net messages in the console for this test.
  await pushPref("devtools.browserconsole.filter.net", true);
  // This is required for testing the text input in the browser console:
  await pushPref("devtools.chrome.enabled", true);

  await addTab(TEST_URI);
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  // Network monitoring is turned off by default in the browser console
  info("Turn on network monitoring");
  await toggleNetworkMonitoringConsoleSetting(hud, true);

  info("Reload the content window to produce a network log");
  const onNetworkMessage = waitForMessageByType(
    hud,
    "test-console.html",
    ".network"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.location.reload();
  });
  const networkMessage = await onNetworkMessage;

  info("Open and check the context menu for the network message");
  let menuPopup = await openContextMenu(hud, networkMessage.node);
  ok(menuPopup, "The context menu is displayed on a network message");

  let expectedContextMenu = addPrefBasedEntries([
    "#console-menu-copy-url (a)",
    "#console-menu-open-url (T)",
    "#console-menu-store (S) [disabled]",
    "#console-menu-copy (C)",
    "#console-menu-copy-object (o) [disabled]",
    "#console-menu-export-clipboard (M)",
    "#console-menu-export-file (F)",
  ]);
  is(
    getSimplifiedContextMenu(menuPopup).join("\n"),
    expectedContextMenu.join("\n"),
    "The context menu has the expected entries for a network message"
  );

  info("Logging a text message in the content window");
  const onLogMessage = waitForMessageByType(
    hud,
    "simple text message",
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.console.log("simple text message");
  });

  const logMessage = await onLogMessage;
  menuPopup = await openContextMenu(hud, logMessage.node);
  ok(menuPopup, "The context menu is displayed on a log message");

  expectedContextMenu = addPrefBasedEntries([
    "#console-menu-store (S) [disabled]",
    "#console-menu-copy (C)",
    "#console-menu-copy-object (o) [disabled]",
    "#console-menu-export-clipboard (M)",
    "#console-menu-export-file (F)",
  ]);
  is(
    getSimplifiedContextMenu(menuPopup).join("\n"),
    expectedContextMenu.join("\n"),
    "The context menu has the expected entries for a simple log message"
  );

  menuPopup = await openContextMenu(hud, hud.jsterm.node);

  let actualEntries = getL10NContextMenu(menuPopup);
  is(
    actualEntries.length,
    6,
    "The context menu has the right number of entries."
  );
  is(actualEntries[0], "#editmenu-undo (text-action-undo) [disabled]");
  is(actualEntries[1], "#editmenu-cut (text-action-cut) [disabled]");
  is(actualEntries[2], "#editmenu-copy (text-action-copy) [disabled]");
  // Paste may or may not be enabled depending on what ran before this.
  // If emptyClipboard is fixed (666254) we could assert if it's enabled/disabled.
  ok(actualEntries[3].startsWith("#editmenu-paste (text-action-paste)"));
  is(actualEntries[4], "#editmenu-delete (text-action-delete) [disabled]");
  is(
    actualEntries[5],
    "#editmenu-selectAll (text-action-select-all) [disabled]"
  );

  const node = hud.jsterm.node;
  const inputContainer = node.closest(".jsterm-input-container");
  await openContextMenu(hud, inputContainer);

  actualEntries = getL10NContextMenu(menuPopup);
  is(
    actualEntries.length,
    6,
    "The context menu has the right number of entries."
  );
  is(actualEntries[0], "#editmenu-undo (text-action-undo) [disabled]");
  is(actualEntries[1], "#editmenu-cut (text-action-cut) [disabled]");
  is(actualEntries[2], "#editmenu-copy (text-action-copy) [disabled]");
  // Paste may or may not be enabled depending on what ran before this.
  // If emptyClipboard is fixed (666254) we could assert if it's enabled/disabled.
  ok(actualEntries[3].startsWith("#editmenu-paste (text-action-paste)"));
  is(actualEntries[4], "#editmenu-delete (text-action-delete) [disabled]");
  is(
    actualEntries[5],
    "#editmenu-selectAll (text-action-select-all) [disabled]"
  );

  await hideContextMenu(hud);
  await toggleNetworkMonitoringConsoleSetting(hud, false);
});

function addPrefBasedEntries(expectedEntries) {
  if (Services.prefs.getBoolPref("devtools.webconsole.sidebarToggle", false)) {
    expectedEntries.push("#console-menu-open-sidebar (V) [disabled]");
  }

  return expectedEntries;
}

function getL10NContextMenu(popupElement) {
  return [...popupElement.querySelectorAll("menuitem")].map(entry => {
    const l10nID = entry.getAttribute("data-l10n-id");
    const disabled = entry.hasAttribute("disabled");
    return `#${entry.id} (${l10nID})${disabled ? " [disabled]" : ""}`;
  });
}

function getSimplifiedContextMenu(popupElement) {
  return [...popupElement.querySelectorAll("menuitem")].map(entry => {
    const key = entry.getAttribute("accesskey");
    const disabled = entry.hasAttribute("disabled");
    return `#${entry.id} (${key})${disabled ? " [disabled]" : ""}`;
  });
}
