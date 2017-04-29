/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for the message timestamps option: check if the preference toggles the
// display of messages in the console output. See bug 722267.

"use strict";

const {PrefObserver} = require("devtools/client/shared/prefs");

const TEST_URI = `data:text/html;charset=utf-8,
  Web Console test for bug 1307871 - preference for toggling timestamps in messages
  <script>
    window.logMessage = function () {
      console.log("simple text message");
    };
  </script>`;
const PREF_MESSAGE_TIMESTAMP = "devtools.webconsole.timestampMessages";

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);

  info("Call the log function defined in the test page");
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.logMessage();
  });

  yield testPrefDefaults(hud);

  let observer = new PrefObserver("");
  let toolbox = gDevTools.getToolbox(hud.target);
  let optionsPanel = yield toolbox.selectTool("options");
  yield togglePref(optionsPanel, observer);
  observer.destroy();

  yield testChangedPref(hud);

  Services.prefs.clearUserPref(PREF_MESSAGE_TIMESTAMP);
});

function* testPrefDefaults(hud) {
  let prefValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
  ok(!prefValue, "Messages should have no timestamp by default (pref check)");
  let message = yield waitFor(() => findMessage(hud, "simple text message"));
  is(message.querySelectorAll(".timestamp").length, 0,
     "Messages should have no timestamp by default (element check)");
}

function* togglePref(panel, observer) {
  info("Options panel opened");

  info("Changing pref");
  let prefChanged = observer.once(PREF_MESSAGE_TIMESTAMP, () => {});
  let checkbox = panel.panelDoc.getElementById("webconsole-timestamp-messages");
  checkbox.click();

  yield prefChanged;
}

function* testChangedPref(hud) {
  let prefValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
  ok(prefValue, "Messages should have timestamps (pref check)");
  let message = yield waitFor(() => findMessage(hud, "simple text message"));
  is(message.querySelectorAll(".timestamp").length, 1,
     "Messages should have timestamp (element check)");
}
