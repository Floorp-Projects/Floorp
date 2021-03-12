/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the ObjectInspector in the sidebar can be navigated with the keyboard.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,
  <script>
    console.log({
      a:1,
      b:2,
      c: Array.from({length: 100}, (_, i) => i)
    });
  </script>`;

add_task(async function() {
  // Should be removed when sidebar work is complete
  await pushPref("devtools.webconsole.sidebarToggle", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  const message = await waitFor(() => findMessage(hud, "Object"));
  const object = message.querySelector(".object-inspector .objectBox-object");

  const onSideBarVisible = waitFor(() =>
    hud.ui.document.querySelector(".sidebar-contents")
  );

  await openObjectInSidebar(hud, object);
  const sidebarContents = await onSideBarVisible;

  const objectInspector = sidebarContents.querySelector(".object-inspector");
  ok(objectInspector, "The ObjectInspector is displayed");

  // There are 5 nodes: the root, a, b, c, and proto.
  await waitFor(() => objectInspector.querySelectorAll(".node").length === 5);
  objectInspector.focus();

  const [root, a, b, c] = objectInspector.querySelectorAll(".node");

  ok(root.classList.contains("focused"), "The root node is focused");

  await synthesizeKeyAndWaitForFocus("KEY_ArrowDown", a);
  ok(true, "`a` node is focused");

  await synthesizeKeyAndWaitForFocus("KEY_ArrowDown", b);
  ok(true, "`b` node is focused");

  await synthesizeKeyAndWaitForFocus("KEY_ArrowDown", c);
  ok(true, "`c` node is focused");

  EventUtils.synthesizeKey("KEY_ArrowRight");
  await waitFor(() => objectInspector.querySelectorAll(".node").length > 5);
  ok(true, "`c` node is expanded");

  const arrayNodes = objectInspector.querySelectorAll(`[aria-level="3"]`);
  await synthesizeKeyAndWaitForFocus("KEY_ArrowDown", arrayNodes[0]);
  ok(true, "First item of the `c` array is focused");

  await synthesizeKeyAndWaitForFocus("KEY_ArrowLeft", c);
  ok(true, "`c` node is focused again");

  await synthesizeKeyAndWaitForFocus("KEY_ArrowUp", b);
  ok(true, "`b` node is focused again");

  info("Select another object in the console output");
  const onArrayMessage = waitForMessage(hud, "Array");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.console.log([4, 5, 6]);
  });

  const arrayMessage = await onArrayMessage;
  const array = arrayMessage.node.querySelector(
    ".object-inspector .objectBox-array"
  );
  await openObjectInSidebar(hud, array);

  await waitFor(() =>
    sidebarContents
      .querySelector(".tree-node")
      .textContent.includes("Array(3) [ 4, 5, 6 ]")
  );
  ok(
    sidebarContents.querySelector(".tree-node").classList.contains("focused"),
    "The root node of the new object in the sidebar is focused"
  );
});

async function openObjectInSidebar(hud, objectNode) {
  const contextMenu = await openContextMenu(hud, objectNode);
  const openInSidebarEntry = contextMenu.querySelector(
    "#console-menu-open-sidebar"
  );
  openInSidebarEntry.click();
  await hideContextMenu(hud);
}

function synthesizeKeyAndWaitForFocus(keyStr, elementToBeFocused) {
  const onFocusChanged = waitFor(() =>
    elementToBeFocused.classList.contains("focused")
  );
  EventUtils.synthesizeKey(keyStr);
  return onFocusChanged;
}
