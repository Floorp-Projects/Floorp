/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the "open" telemetry event is correctly logged when opening the
// toolbox.
const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

add_task(async function () {
  Services.prefs.clearUserPref("devtools.toolbox.selectedTool");
  const tab = await addTab("data:text/html;charset=utf-8,Test open event");

  info("Open the toolbox with a shortcut to trigger the open event");
  const onToolboxReady = gDevTools.once("toolbox-ready");
  EventUtils.synthesizeKey("VK_F12", {});
  await onToolboxReady;

  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  // The telemetry is sent by DevToolsStartup and so isn't flaged against any session id
  const events = snapshot.parent.filter(
    event =>
      event[1] === "devtools.main" &&
      event[2] === "open" &&
      event[5].session_id == -1
  );

  is(events.length, 1, "Telemetry open event was logged");

  const extras = events[0][5];
  is(extras.entrypoint, "KeyShortcut", "entrypoint extra is correct");
  // The logged shortcut is `${modifiers}+${shortcut}`, which adds an
  // extra `+` before F12 here.
  // See https://searchfox.org/mozilla-central/rev/c7e8bc4996f979e5876b33afae3de3b1ab4f3ae1/devtools/startup/DevToolsStartup.jsm#1070
  is(extras.shortcut, "+F12", "entrypoint shortcut is correct");

  gBrowser.removeTab(tab);
});
