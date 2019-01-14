/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
`data:text/html;charset=utf-8,` +
`<script>window.logStuff = function () {console.log("hello");};</script>`;
const TEST_DATA = {
  msg: "simple text message",
  msg2: "second message test",
};

// Test the export visible messages to clipboard of the webconsole copies the expected
// clipboard text for different log messages to find if everything is copied to clipboard.

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();

  info("Call the log function defined in the test page");
  await ContentTask.spawn(gBrowser.selectedBrowser, TEST_DATA, function(testData) {
    content.wrappedJSObject.console.log(testData.msg);
    content.wrappedJSObject.console.log(testData.msg2);
    content.wrappedJSObject.console.log("object:", {a: 1},
                                        "array:", ["b", "c"]);
    content.wrappedJSObject.logStuff();
  });

  info("Test export to clipboard ");
  await waitFor(() => findMessages(hud, "").length === 4);
  const message = findMessage(hud, TEST_DATA.msg);
  const clipboardText = await exportAllToClipboard(hud, message);
  ok(true, "Clipboard text was found and saved");

  const clipboardLines = clipboardText.split("\n");
  info("Check if all messages where copied to clipboard");
  is(clipboardLines[0].trim(), TEST_DATA.msg,
    "found first text message in clipboard");
  is(clipboardLines[1].trim(), TEST_DATA.msg2,
    "found second text message in clipboard");
  is(clipboardLines[2].trim(), 'object: Object { a: 1 } array: Array [ "b", "c" ]',
    "found object and array in clipboard");
  const CLEAN_URI = TEST_URI.replace("text/html;charset=utf-8,", "");
  is(clipboardLines[3].trim(), `hello ${CLEAN_URI}:1:32`,
    "found text from data uri");
});

/**
 * Simple helper method to open the context menu on a given message, and click on the
 * export visible messages to clipboard.
 */
async function exportAllToClipboard(hud, message) {
  const menuPopup = await openContextMenu(hud, message);
  const exportClipboard = menuPopup.querySelector("#console-menu-export-clipboard");
  ok(exportClipboard, "copy menu item is enabled");

  let clipboardText;
  await waitForClipboardPromise(
    () => exportClipboard.click(),
    data => {
      clipboardText = data;
      return data;
    }
  );
  return clipboardText;
}
