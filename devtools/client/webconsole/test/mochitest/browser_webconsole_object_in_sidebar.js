/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the ObjectInspector is rendered correctly in the sidebar.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8," +
  "<script>console.log({a:1,b:2,c:3});</script>";

add_task(async function() {
  // Should be removed when sidebar work is complete
  await pushPref("devtools.webconsole.sidebarToggle", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  const message = findMessage(hud, "Object");
  const object = message.querySelector(".object-inspector .objectBox-object");

  await showSidebarWithContextMenu(hud, object, true);

  const sidebarContents = hud.ui.document.querySelector(".sidebar-contents");
  const objectInspectors = [...sidebarContents.querySelectorAll(".tree")];
  is(objectInspectors.length, 1, "There is the expected number of object inspectors");
  const [objectInspector] = objectInspectors;
  let oiNodes = objectInspector.querySelectorAll(".node");
  if (oiNodes.length === 1) {
    // If this is the case, we wait for the properties to be fetched and displayed.
    await waitForNodeMutation(objectInspector, {
      childList: true
    });
    oiNodes = objectInspector.querySelectorAll(".node");
  }

  // There are 5 nodes: the root, a, b, c, and proto.
  is(oiNodes.length, 5, "There is the expected number of nodes in the tree");
  const propertiesNodes = [...objectInspector.querySelectorAll(".object-label")]
    .map(el => el.textContent);
  const arrayPropertiesNames = ["a", "b", "c", "<prototype>"];
  is(JSON.stringify(propertiesNodes), JSON.stringify(arrayPropertiesNames));
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
