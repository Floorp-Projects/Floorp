/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for the message timestamps option: check if the preference toggles the
// display of messages in the console output. See bug 722267.

"use strict";

const { PrefObserver } = require("devtools/client/shared/prefs");

const TEST_URI = `data:text/html;charset=utf-8,
  Web Console test for bug 1307871 - preference for toggling timestamps in messages`;
const PREF_MESSAGE_TIMESTAMP = "devtools.webconsole.timestampMessages";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Call the log function defined in the test page");
  const onMessage = waitForMessage(hud, "simple text message");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.console.log("simple text message");
  });
  const message = await onMessage;

  const prefValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
  ok(!prefValue, "Messages should have no timestamp by default (pref check)");
  ok(
    !message.node.querySelector(".timestamp"),
    "Messages should have no timestamp by default (element check)"
  );

  const observer = new PrefObserver("");

  info("Change Timestamp preference");
  const prefChanged = observer.once(PREF_MESSAGE_TIMESTAMP, () => {});

  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-timestamps"
  );

  await prefChanged;
  observer.destroy();

  ok(
    message.node.querySelector(".timestamp"),
    "Messages should have timestamp"
  );

  Services.prefs.clearUserPref(PREF_MESSAGE_TIMESTAMP);
});
