/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>Web Console test for bug 1567372";

add_task(async function () {
  await pushPref("devtools.webconsole.input.editor", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Searching for `.webconsole-editor-toolbar`");
  const editorToolbar = hud.ui.outputNode.querySelector(
    ".webconsole-editor-toolbar"
  );

  info("Searching for `.webconsole-editor-toolbar-reverseSearchButton`");
  const reverseSearchButton = editorToolbar.querySelector(
    ".webconsole-editor-toolbar-reverseSearchButton"
  );

  const onReverseSearchUiOpen = waitFor(
    () => getReverseSearchElement(hud) != null
  );

  info("Performing click on `.webconsole-editor-toolbar-reverseSearchButton`");
  reverseSearchButton.click();

  await onReverseSearchUiOpen;
  ok(true, "Reverse Search UI is open");

  ok(
    reverseSearchButton.classList.contains("checked"),
    "Reverse Search Button is marked as checked"
  );

  const onReverseSearchUiClosed = waitFor(
    () => getReverseSearchElement(hud) == null
  );

  info("Performing click on `.webconsole-editor-toolbar-reverseSearchButton`");
  reverseSearchButton.click();

  await onReverseSearchUiClosed;
  ok(true, "Reverse Search UI is closed");

  ok(
    !reverseSearchButton.classList.contains("checked"),
    "Reverse Search Button is NOT marked as checked"
  );
});
