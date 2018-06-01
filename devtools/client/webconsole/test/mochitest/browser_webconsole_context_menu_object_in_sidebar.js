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

add_task(async function() {
  // Should be removed when sidebar work is complete
  await pushPref("devtools.webconsole.sidebarToggle", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  const message = findMessage(hud, "foo");
  const [objectA, objectB] =
    message.querySelectorAll(".object-inspector .objectBox-object");
  const number = findMessage(hud, "100", ".objectBox");
  const string = findMessage(hud, "foo", ".objectBox");
  const bool = findMessage(hud, "false", ".objectBox");
  const nullMessage = findMessage(hud, "null", ".objectBox");
  const undefinedMsg = findMessage(hud, "undefined", ".objectBox");

  info("Showing sidebar for {a:1}");
  await showSidebarWithContextMenu(hud, objectA, true);

  let sidebarContents = hud.ui.document.querySelector(".sidebar-contents");
  let objectInspector = sidebarContents.querySelector(".object-inspector");
  let oiNodes = objectInspector.querySelectorAll(".node");
  if (oiNodes.length === 1) {
    // If this is the case, we wait for the properties to be fetched and displayed.
    await waitForNodeMutation(objectInspector, {
      childList: true
    });
  }

  let sidebarText = hud.ui.document.querySelector(".sidebar-contents").textContent;
  ok(sidebarText.includes("a: 1"), "Sidebar is shown for {a:1}");

  info("Showing sidebar for {a:1} again");
  await showSidebarWithContextMenu(hud, objectA, false);
  ok(hud.ui.document.querySelector(".sidebar"),
     "Sidebar is still shown after clicking on same object");
  is(hud.ui.document.querySelector(".sidebar-contents").textContent, sidebarText,
     "Sidebar is not updated after clicking on same object");

  info("Showing sidebar for {b:1}");
  await showSidebarWithContextMenu(hud, objectB, false);

  sidebarContents = hud.ui.document.querySelector(".sidebar-contents");
  objectInspector = sidebarContents.querySelector(".object-inspector");
  oiNodes = objectInspector.querySelectorAll(".node");
  if (oiNodes.length === 1) {
    // If this is the case, we wait for the properties to be fetched and displayed.
    await waitForNodeMutation(objectInspector, {
      childList: true
    });
  }

  isnot(hud.ui.document.querySelector(".sidebar-contents").textContent, sidebarText,
        "Sidebar is updated for {b:1}");
  sidebarText = hud.ui.document.querySelector(".sidebar-contents").textContent;

  ok(sidebarText.includes("b: 1"), "Sidebar contents shown for {b:1}");

  info("Checking context menu entry is disabled for number");
  const numberContextMenuEnabled = await isContextMenuEntryEnabled(hud, number);
  ok(!numberContextMenuEnabled, "Context menu entry is disabled for number");

  info("Checking context menu entry is disabled for string");
  const stringContextMenuEnabled = await isContextMenuEntryEnabled(hud, string);
  ok(!stringContextMenuEnabled, "Context menu entry is disabled for string");

  info("Checking context menu entry is disabled for bool");
  const boolContextMenuEnabled = await isContextMenuEntryEnabled(hud, bool);
  ok(!boolContextMenuEnabled, "Context menu entry is disabled for bool");

  info("Checking context menu entry is disabled for null message");
  const nullContextMenuEnabled = await isContextMenuEntryEnabled(hud, nullMessage);
  ok(!nullContextMenuEnabled, "Context menu entry is disabled for nullMessage");

  info("Checking context menu entry is disabled for undefined message");
  const undefinedContextMenuEnabled = await isContextMenuEntryEnabled(hud, undefinedMsg);
  ok(!undefinedContextMenuEnabled, "Context menu entry is disabled for undefinedMsg");
});

async function showSidebarWithContextMenu(hud, node, expectMutation) {
  const wrapper = hud.ui.document.querySelector(".webconsole-output-wrapper");
  const onSidebarShown = waitForNodeMutation(wrapper, { childList: true });

  const contextMenu = await openContextMenu(hud, node);
  const openInSidebar = contextMenu.querySelector("#console-menu-open-sidebar");
  openInSidebar.click();
  if (expectMutation) {
    await onSidebarShown;
  }
  await hideContextMenu(hud);
}

async function isContextMenuEntryEnabled(hud, node) {
  const contextMenu = await openContextMenu(hud, node);
  const openInSidebar = contextMenu.querySelector("#console-menu-open-sidebar");
  const enabled = !openInSidebar.attributes.disabled;
  await hideContextMenu(hud);
  return enabled;
}
