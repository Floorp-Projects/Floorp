/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that an object in the sidebar can still be expanded after the message where it was
// logged is pruned.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8," +
  "<script>console.log({a:1,b:2,c:[3,4,5]});</script>";

add_task(async function() {
  // Should be removed when sidebar work is complete (Bug 1447235)
  await pushPref("devtools.webconsole.sidebarToggle", true);
  // Set the loglimit to 1 so message gets pruned as soon as another message is displayed.
  await pushPref("devtools.hud.loglimit", 1);

  const hud = await openNewTabAndConsole(TEST_URI);

  const message = await waitFor(() => findMessage(hud, "Object"));
  const object = message.querySelector(".object-inspector .objectBox-object");

  const sidebar = await showSidebarWithContextMenu(hud, object, true);

  const oi = sidebar.querySelector(".object-inspector");
  let oiNodes = oi.querySelectorAll(".node");
  if (oiNodes.length === 1) {
    // If this is the case, we wait for the properties to be fetched and displayed.
    await waitFor(() => oi.querySelectorAll(".node").length > 1);
    oiNodes = oi.querySelectorAll(".node");
  }

  info("Log a message so the original one gets pruned");
  const messageText = "hello world";
  const onMessage = waitForMessage(hud, messageText);
  ContentTask.spawn(gBrowser.selectedBrowser, messageText, async function(str) {
    content.console.log(str);
  });
  await onMessage;

  ok(!findMessage(hud, "Object"), "Message with object was pruned");

  info("Expand the 'c' node in the sidebar");
  // Here's what the object in the sidebar looks like:
  // ▼ {…}
  //     a: 1
  //     b: 2
  //   ▶︎ c: (3) […]
  //   ▶︎ <prototype>: {…}
  const cNode = oiNodes[3];
  const onNodeExpanded = waitFor(() => oi.querySelectorAll(".node").length > 5);
  cNode.click();
  await onNodeExpanded;

  // Here's what the object in the sidebar should look like:
  // ▼ {…}
  //     a: 1
  //     b: 2
  //   ▼ c: (3) […]
  //       0: 3
  //       1: 4
  //       2: 5
  //       length: 3
  //     ▶︎ <prototype>: []
  //   ▶︎ <prototype>: {…}
  is(oi.querySelectorAll(".node").length, 10, "The 'c' property was expanded");
});

async function showSidebarWithContextMenu(hud, node) {
  const appNode = hud.ui.document.querySelector(".webconsole-app");
  const onSidebarShown = waitFor(() => appNode.querySelector(".sidebar"));

  const contextMenu = await openContextMenu(hud, node);
  const openInSidebar = contextMenu.querySelector("#console-menu-open-sidebar");
  openInSidebar.click();
  await onSidebarShown;
  await hideContextMenu(hud);
  return onSidebarShown;
}
