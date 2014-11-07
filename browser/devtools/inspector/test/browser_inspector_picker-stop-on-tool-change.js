/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the highlighter's picker is stopped when a different tool is
// selected

const TEST_URI = "data:text/html;charset=UTF-8," +
  "testing the highlighter goes away on tool selection";

add_task(function* () {
  let { toolbox } = yield openInspectorForURL(TEST_URI);
  let pickerStopped = toolbox.once("picker-stopped");

  info("Starting the inspector picker");
  yield toolbox.highlighterUtils.startPicker();

  info("Selecting another tool than the inspector in the toolbox");
  yield toolbox.selectNextTool();

  info("Waiting for the picker-stopped event to be fired");
  yield pickerStopped;

  ok(true, "picker-stopped event fired after switch tools; picker is closed");
});
