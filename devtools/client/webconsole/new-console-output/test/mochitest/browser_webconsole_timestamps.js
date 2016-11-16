/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for the message timestamps option: check if the preference toggles the
// display of messages in the console output. See bug 722267.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 1307871 - preference for toggling timestamps in messages";
const PREF_MESSAGE_TIMESTAMP = "devtools.webconsole.timestampMessages";

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);
  let outputNode = hud.ui.experimentalOutputNode;
  let outputEl = outputNode.querySelector(".webconsole-output");

  testPrefDefaults(outputEl);

  let toolbox = gDevTools.getToolbox(hud.target);
  let optionsPanel = yield toolbox.selectTool("options");
  yield togglePref(optionsPanel);

  yield testChangedPref(outputEl);

  Services.prefs.clearUserPref(PREF_MESSAGE_TIMESTAMP);
});

function testPrefDefaults(outputEl) {
  let prefValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
  ok(!prefValue, "Messages should have no timestamp by default (pref check)");
  ok(outputEl.classList.contains("hideTimestamps"),
     "Messages should have no timestamp (class name check)");
}

function* togglePref(panel) {
  info("Options panel opened");

  info("Changing pref");
  let prefChanged = new Promise(resolve => {
    gDevTools.once("pref-changed", resolve);
  });
  let checkbox = panel.panelDoc.getElementById("webconsole-timestamp-messages");
  checkbox.click();

  yield prefChanged;
}

function* testChangedPref(outputEl) {
  let prefValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
  ok(prefValue, "Messages should have timestamps (pref check)");
  ok(!outputEl.classList.contains("hideTimestamps"),
     "Messages should have timestamps (class name check)");
}
