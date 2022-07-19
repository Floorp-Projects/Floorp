/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html,<!DOCTYPE html><meta charset=utf8>console API calls<script>
  console.log({
    contentObject: "YAY!",
    deep: ["hello", "world"]
  });
</script>`;

add_task(async function() {
  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);
  // Enable Fission browser console to see the logged content object
  await pushPref("devtools.browsertoolbox.fission", true);

  await addTab(TEST_URI);

  info("Open the Browser Console");
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Wait until the content object is displayed");
  const message = await waitFor(() =>
    findConsoleAPIMessage(
      hud,
      `Object { contentObject: "YAY!", deep: (2) […] }`
    )
  );
  ok(true, "Content object is displayed in the Browser Console");
  // Clear clipboard content.
  SpecialPowers.clipboardCopyString("");

  const menuPopup = await openContextMenu(hud, message);
  const exportClipboard = menuPopup.querySelector(
    "#console-menu-export-clipboard"
  );
  ok(exportClipboard, "copy menu item is enabled");

  const clipboardText = await waitForClipboardPromise(
    () => exportClipboard.click(),
    data => data.includes("YAY")
  );
  menuPopup.hidePopup();

  ok(true, "Clipboard text was found and saved");
  // We're only checking that the export did work.
  // browser_webconsole_context_menu_export_console_output.js covers the feature in
  // greater detail.
  ok(
    clipboardText.includes(`Object { contentObject: "YAY!", deep: (2) […] }`),
    "Message was exported to clipboard"
  );
});
