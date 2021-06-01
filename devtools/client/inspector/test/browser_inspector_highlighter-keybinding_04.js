/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that pressing ESC twice while in picker mode first stops the picker and
// then opens the split-console (see bug 988278).

const TEST_URL = "data:text/html;charset=utf8,<div></div>";

add_task(async function() {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  await startPicker(toolbox);

  info("Start using the picker by hovering over nodes");
  const onHover = toolbox.nodePicker.once("picker-node-hovered");
  await safeSynthesizeMouseEventAtCenterInContentPage("div", {
    type: "mousemove",
  });

  await onHover;

  info("Press escape and wait for the picker to stop");
  const onPickerStopped = toolbox.nodePicker.once("picker-node-canceled");
  BrowserTestUtils.synthesizeKey("VK_ESCAPE", {}, gBrowser.selectedBrowser);

  await onPickerStopped;

  info("Press escape again and wait for the split console to open");
  const onSplitConsole = toolbox.once("split-console");
  const onConsoleReady = toolbox.once("webconsole-ready");
  // The escape key is synthesized in the main process, which is where the focus
  // should be after the picker was stopped.
  EventUtils.synthesizeKey("VK_ESCAPE", {}, inspector.panelWin);
  await onSplitConsole;
  await onConsoleReady;
  ok(toolbox.splitConsole, "The split console is shown.");

  // Hide the split console.
  await toolbox.toggleSplitConsole();
});
