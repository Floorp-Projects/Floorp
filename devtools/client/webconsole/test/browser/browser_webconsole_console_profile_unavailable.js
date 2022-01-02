/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  waitForRecordingStartedEvents,
  waitForRecordingStoppedEvents,
} = require("devtools/client/performance/test/helpers/actions");

// Check console.profile() shows a warning with the new performance panel.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>test console.profile</h1>";

const EXPECTED_WARNING =
  "console.profile is not compatible with the new Performance recorder";

add_task(async function consoleProfileWarningWithNewPerfPanel() {
  await pushPref("devtools.performance.new-panel-enabled", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Use console.profile in the content page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.profile();
  });

  await waitFor(
    () => findMessage(hud, EXPECTED_WARNING),
    "Wait until the warning about console.profile is displayed"
  );
  ok(true, "The expected warning was displayed.");
});

// Check console.profile() shows no warning with the old performance panel.
add_task(async function consoleProfileNoWarningWithOldPerfPanel() {
  await pushPref("devtools.performance.new-panel-enabled", false);
  const hud = await openNewTabAndConsole(TEST_URI);

  await hud.toolbox.loadTool("performance");
  const perfPanel = hud.toolbox.getPanel("performance");

  const profileStarted = waitForRecordingStartedEvents(perfPanel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
  });
  const profileStopped = waitForRecordingStoppedEvents(perfPanel, {
    // only emitted for manual recordings
    skipWaitingForBackendReady: true,
  });

  info("Use console.profile in the content page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.profile();
  });
  await profileStarted;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.profileEnd();
  });
  await profileStopped;

  // Wait for one second to give some time to the warning to be displayed.
  await wait(1000);

  ok(
    !findMessage(hud, EXPECTED_WARNING),
    "The console.profile warning was not displayed."
  );
});
