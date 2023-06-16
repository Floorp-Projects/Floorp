/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test to check the 'Copy URL' functionality in the context menu item for stylesheets.

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function () {
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);

  const doc = panel.panelWindow.document;
  const contextMenu = getContextMenuElement(panel);
  const copyUrlItem = doc.getElementById("context-copyurl");

  const onContextMenuShown = new Promise(resolve => {
    contextMenu.addEventListener("popupshown", resolve, { once: true });
  });

  info("Right-click the first stylesheet editor.");
  const editor = ui.editors[0];

  is(editor.friendlyName, "simple.css", "editor is the expected one");

  const stylesheetEl = editor.summary.querySelector(".stylesheet-name");
  await EventUtils.synthesizeMouseAtCenter(
    stylesheetEl,
    { button: 2, type: "contextmenu" },
    panel.panelWindow
  );
  await onContextMenuShown;

  ok(!copyUrlItem.hidden, "Copy URL menu item should be showing.");

  info(
    "Click on Copy URL menu item and wait for the URL to be copied to the clipboard."
  );
  await waitForClipboardPromise(
    () => contextMenu.activateItem(copyUrlItem),
    `${TEST_BASE_HTTPS}simple.css`
  );
});
