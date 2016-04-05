/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that pressing ESC when a node in the markup-view is focused toggles
// the split-console (see bug 988278)

const TEST_URL = "data:text/html;charset=utf8,<div></div>";

add_task(function* () {
  let {inspector, toolbox} = yield openInspectorForURL(TEST_URL);

  info("Focusing the tag editor of the test element");
  let {editor} = yield getContainerForSelector("div", inspector);
  editor.tag.focus();

  info("Pressing ESC and wait for the split-console to open");
  let onSplitConsole = toolbox.once("split-console");
  let onConsoleReady = toolbox.once("webconsole-ready");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, inspector.panelWin);
  yield onSplitConsole;
  yield onConsoleReady;
  ok(toolbox.splitConsole, "The split console is shown.");

  info("Pressing ESC again and wait for the split-console to close");
  onSplitConsole = toolbox.once("split-console");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, inspector.panelWin);
  yield onSplitConsole;
  ok(!toolbox.splitConsole, "The split console is hidden.");
});
