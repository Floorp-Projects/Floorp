/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the "Open in sidebar" context menu entry is active for
// the correct objects and opens the sidebar when clicked.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8," +
  "<script>console.log({a:1},100,{b:1},'foo',false,null,undefined);</script>";

add_task(async function () {
  // Should be removed when sidebar work is complete
  await pushPref("devtools.webconsole.sidebarToggle", true);

  let hud = await openNewTabAndConsole(TEST_URI);

  let message = findMessage(hud, "foo");
  let [objectA, objectB] =
    message.querySelectorAll(".object-inspector .objectBox-object");
  let number = findMessage(hud, "100", ".objectBox");
  let string = findMessage(hud, "foo", ".objectBox");
  let bool = findMessage(hud, "false", ".objectBox");
  let nullMessage = findMessage(hud, "null", ".objectBox");
  let undefinedMsg = findMessage(hud, "undefined", ".objectBox");

  info("Showing sidebar for {a:1}");
  await showSidebarWithContextMenu(hud, objectA, true);

  let sidebarText = hud.ui.document.querySelector(".sidebar-contents").textContent;
  ok(sidebarText.includes('"a":'), "Sidebar is shown for {a:1}");

  info("Showing sidebar for {a:1} again");
  await showSidebarWithContextMenu(hud, objectA, false);
  ok(hud.ui.document.querySelector(".sidebar"),
     "Sidebar is still shown after clicking on same object");
  is(hud.ui.document.querySelector(".sidebar-contents").textContent, sidebarText,
     "Sidebar is not updated after clicking on same object");

  info("Showing sidebar for {b:1}");
  await showSidebarWithContextMenu(hud, objectB, false);
  isnot(hud.ui.document.querySelector(".sidebar-contents").textContent, sidebarText,
        "Sidebar is updated for {b:1}");
  sidebarText = hud.ui.document.querySelector(".sidebar-contents").textContent;
  ok(sidebarText.includes('"b":'), "Sidebar contents shown for {b:1}");

  info("Checking context menu entry is disabled for number");
  let numberContextMenuEnabled = await isContextMenuEntryEnabled(hud, number);
  ok(!numberContextMenuEnabled, "Context menu entry is disabled for number");

  info("Checking context menu entry is disabled for string");
  let stringContextMenuEnabled = await isContextMenuEntryEnabled(hud, string);
  ok(!stringContextMenuEnabled, "Context menu entry is disabled for string");

  info("Checking context menu entry is disabled for bool");
  let boolContextMenuEnabled = await isContextMenuEntryEnabled(hud, bool);
  ok(!boolContextMenuEnabled, "Context menu entry is disabled for bool");

  info("Checking context menu entry is disabled for null message");
  let nullContextMenuEnabled = await isContextMenuEntryEnabled(hud, nullMessage);
  ok(!nullContextMenuEnabled, "Context menu entry is disabled for nullMessage");

  info("Checking context menu entry is disabled for undefined message");
  let undefinedContextMenuEnabled = await isContextMenuEntryEnabled(hud, undefinedMsg);
  ok(!undefinedContextMenuEnabled, "Context menu entry is disabled for undefinedMsg");
});

async function showSidebarWithContextMenu(hud, node, expectMutation) {
  let wrapper = hud.ui.document.querySelector(".webconsole-output-wrapper");
  let onSidebarShown = waitForNodeMutation(wrapper, { childList: true });

  let contextMenu = await openContextMenu(hud, node);
  let openInSidebar = contextMenu.querySelector("#console-menu-open-sidebar");
  openInSidebar.click();
  if (expectMutation) {
    await onSidebarShown;
  }
  await hideContextMenu(hud);
}

async function isContextMenuEntryEnabled(hud, node) {
  let contextMenu = await openContextMenu(hud, node);
  let openInSidebar = contextMenu.querySelector("#console-menu-open-sidebar");
  let enabled = !openInSidebar.attributes.disabled;
  await hideContextMenu(hud);
  return enabled;
}
