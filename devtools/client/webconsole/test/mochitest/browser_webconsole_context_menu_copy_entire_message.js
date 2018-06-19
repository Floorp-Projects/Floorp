/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PrefObserver} = require("devtools/client/shared/prefs");

// RegExp that validates copied text for log lines.
const LOG_FORMAT_WITH_TIMESTAMP = /^[\d:.]+ .+ (\d+ )?.+:\d+$/;
const LOG_FORMAT_WITHOUT_TIMESTAMP = /^.+ (\d+ )?.+:\d+$/;
// RegExp that validates copied text for stacktrace lines.
const TRACE_FORMAT = /^\t.+ .+:\d+:\d+$/;

const PREF_MESSAGE_TIMESTAMP = "devtools.webconsole.timestampMessages";

const TEST_URI = `data:text/html;charset=utf-8,<script>
  window.logStuff = function () {
    console.log("simple " +  "text message");
    function wrapper() {
      console.trace();
    }
    wrapper();
  };
</script>`;

// Test the Copy menu item of the webconsole copies the expected clipboard text for
// different log messages.

add_task(async function() {
  const observer = new PrefObserver("");
  let onPrefUpdated = observer.once(PREF_MESSAGE_TIMESTAMP, () => {});
  Services.prefs.setBoolPref(PREF_MESSAGE_TIMESTAMP, true);
  await onPrefUpdated;

  const hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();

  info("Call the log function defined in the test page");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.logStuff();
  });

  info("Test copy menu item for the simple log");
  let message = await waitFor(() => findMessage(hud, "simple text message"));
  let clipboardText = await copyMessageContent(hud, message);
  ok(true, "Clipboard text was found and saved");

  info("Check copied text for simple log message");
  let lines = clipboardText.split("\n");
  is(lines.length, 2, "There are 2 lines in the copied text");
  is(lines[1], "", "The last line is an empty new line");
  ok(LOG_FORMAT_WITH_TIMESTAMP.test(lines[0]),
    "Log line has the right format:\n" + lines[0]);

  info("Test copy menu item for the stack trace message");
  message = await waitFor(() => findMessage(hud, "console.trace"));
  clipboardText = await copyMessageContent(hud, message);
  ok(true, "Clipboard text was found and saved");

  info("Check copied text for stack trace message");
  lines = clipboardText.split("\n");
  is(lines.length, 4, "There are 4 lines in the copied text");
  is(lines[3], "", "The last line is an empty new line");
  ok(LOG_FORMAT_WITH_TIMESTAMP.test(lines[0]),
    "Log line has the right format:\n" + lines[0]);
  ok(TRACE_FORMAT.test(lines[1]), "Stacktrace line has the right format:\n" + lines[1]);
  ok(TRACE_FORMAT.test(lines[2]), "Stacktrace line has the right format:\n" + lines[2]);

  info("Test copy menu item without timestamp");

  onPrefUpdated = observer.once(PREF_MESSAGE_TIMESTAMP, () => {});
  Services.prefs.setBoolPref(PREF_MESSAGE_TIMESTAMP, false);
  await onPrefUpdated;

  info("Test copy menu item for the simple log");
  message = await waitFor(() => findMessage(hud, "simple text message"));
  clipboardText = await copyMessageContent(hud, message);
  ok(true, "Clipboard text was found and saved");

  info("Check copied text for simple log message");
  lines = clipboardText.split("\n");
  is(lines.length, 2, "There are 2 lines in the copied text");
  is(lines[1], "", "The last line is an empty new line");
  ok(LOG_FORMAT_WITHOUT_TIMESTAMP.test(lines[0]),
    "Log line has the right format:\n" + lines[0]);

  info("Test copy menu item for the stack trace message");
  message = await waitFor(() => findMessage(hud, "console.trace"));
  clipboardText = await copyMessageContent(hud, message);
  ok(true, "Clipboard text was found and saved");

  info("Check copied text for stack trace message");
  lines = clipboardText.split("\n");
  is(lines.length, 4, "There are 4 lines in the copied text");
  is(lines[3], "", "The last line is an empty new line");
  ok(LOG_FORMAT_WITHOUT_TIMESTAMP.test(lines[0]),
    "Log line has the right format:\n" + lines[0]);
  ok(TRACE_FORMAT.test(lines[1]), "Stacktrace line has the right format:\n" + lines[1]);
  ok(TRACE_FORMAT.test(lines[2]), "Stacktrace line has the right format:\n" + lines[2]);

  observer.destroy();
  Services.prefs.clearUserPref(PREF_MESSAGE_TIMESTAMP);
});

/**
 * Simple helper method to open the context menu on a given message, and click on the copy
 * menu item.
 */
async function copyMessageContent(hud, message) {
  const menuPopup = await openContextMenu(hud, message);
  const copyMenuItem = menuPopup.querySelector("#console-menu-copy");
  ok(copyMenuItem, "copy menu item is enabled");

  let clipboardText;
  await waitForClipboardPromise(
    () => copyMenuItem.click(),
    data => {
      clipboardText = data;
      return data === message.textContent;
    }
  );
  return clipboardText;
}
