/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that the highlighter's picker should be stopped when the toolbox is
// closed

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>testing the highlighter goes away on destroy</p>";

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URI);
  let pickerStopped = toolbox.once("picker-stopped");

  yield selectNode("p", inspector);

  info("Inspector displayed and ready, starting the picker.");
  yield startPicker(toolbox);

  info("Destroying the toolbox.");
  yield toolbox.destroy();

  info("Waiting for the picker-stopped event that should be fired when the " +
       "toolbox is destroyed.");
  yield pickerStopped;

  ok(true, "picker-stopped event fired after switch tools so picker is closed");
});
