"use strict";

// On debug builds, crashing tabs results in much thinking, which
// slows down the test and results in intermittent test timeouts,
// so we'll pump up the expected timeout for this test.
requestLongerTimeout(2);

SimpleTest.expectChildProcessCrash();

add_task(async function test_browser_crashed_false_positive_event() {
  info("Waiting for oop-browser-crashed event.");

  Services.telemetry.clearScalars();
  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );

  ok(await ensureBuildID(), "System has correct platform.ini");
  setBuildidMatchDontSendEnv();
  await forceCleanProcesses();
  let eventPromise = getEventPromise("oop-browser-crashed", "false-positive");
  let tab = await openNewTab(false);
  await eventPromise;
  unsetBuildidMatchDontSendEnv();

  is(
    getFalsePositiveTelemetry(),
    1,
    "Build ID mismatch false positive count should be 1"
  );

  await closeTab(tab);
});
