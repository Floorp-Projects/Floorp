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

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Call the log function defined in the test page");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.logMessage();
  });

  await testPrefDefaults(hud);

  const observer = new PrefObserver("");
  const toolbox = gDevTools.getToolbox(hud.target);
  const optionsPanel = await toolbox.selectTool("options");
  await togglePref(optionsPanel, observer);
  observer.destroy();

  // Switch back to the console as it won't update when it is in background
  await toolbox.selectTool("webconsole");

  await testChangedPref(hud);

  Services.prefs.clearUserPref(PREF_MESSAGE_TIMESTAMP);
});

async function testPrefDefaults(hud) {
  const prefValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
  ok(!prefValue, "Messages should have no timestamp by default (pref check)");
  const message = await waitFor(() => findMessage(hud, "simple text message"));
  is(message.querySelectorAll(".timestamp").length, 0,
     "Messages should have no timestamp by default (element check)");
}

async function togglePref(panel, observer) {
  info("Options panel opened");

  info("Changing pref");
  const prefChanged = observer.once(PREF_MESSAGE_TIMESTAMP, () => {});
  const checkbox = panel.panelDoc.getElementById("webconsole-timestamp-messages");
  checkbox.click();

  await prefChanged;
}

async function testChangedPref(hud) {
  const prefValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
  ok(prefValue, "Messages should have timestamps (pref check)");
  const message = await waitFor(() => findMessage(hud, "simple text message"));
  is(message.querySelectorAll(".timestamp").length, 1,
     "Messages should have timestamp (element check)");
}
