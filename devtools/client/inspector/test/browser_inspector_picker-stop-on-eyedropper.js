/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the highlighter's picker is stopped when the eyedropper tool is
// selected

const TEST_URI =
  "data:text/html;charset=UTF-8," +
  "testing the highlighter goes away on eyedropper selection";

add_task(async function () {
  const { toolbox } = await openInspectorForURL(TEST_URI);
  const pickerStopped = toolbox.nodePicker.once("picker-stopped");
  const eyeDropperButtonClasses =
    toolbox.getPanel("inspector").eyeDropperButton.classList;

  const eyeDropperStopped = waitFor(
    () => !eyeDropperButtonClasses.contains("checked")
  );

  info("Starting the inspector picker");
  await startPicker(toolbox);

  info("Starting the eyedropper tool");
  await startEyeDropper(toolbox);

  ok(eyeDropperButtonClasses.contains("checked"), "eyedropper is started");

  info("Waiting for the picker-stopped event to be fired");
  await pickerStopped;

  ok(
    true,
    "picker-stopped event fired after eyedropper is selected; picker is closed"
  );

  info("Starting the inspector picker again");
  await startPicker(toolbox);

  info("Checking if eyedropper is stopped");
  await eyeDropperStopped;

  ok(true, "eyedropper stopped after node picker; eyedropper is closed");
});
