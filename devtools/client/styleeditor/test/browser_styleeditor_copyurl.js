/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test to check the 'Copy URL' functionality in the context menu item for stylesheets.

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  const onContextMenuShown = new Promise(resolve => {
    ui._contextMenu.addEventListener(
      "popupshown",
      () => {
        resolve();
      },
      { once: true }
    );
  });

  info("Right-click the first stylesheet editor.");
  const editor = ui.editors[0];
  const stylesheetEl = editor.summary.querySelector(".stylesheet-name");
  await EventUtils.synthesizeMouseAtCenter(
    stylesheetEl,
    { button: 2, type: "contextmenu" },
    ui._window
  );
  await onContextMenuShown;

  ok(!ui._copyUrlItem.hidden, "Copy URL menu item should be showing.");

  info(
    "Click on Copy URL menu item and wait for the URL to be copied to the clipboard."
  );
  await waitForClipboardPromise(
    () => ui._contextMenu.activateItem(ui._copyUrlItem),
    ui._contextMenuStyleSheet.href
  );
});
