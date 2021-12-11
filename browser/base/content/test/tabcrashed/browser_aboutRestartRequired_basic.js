"use strict";

const kTimeout = 5 * 1000;

add_task(async function test_browser_crashed_basic_event() {
  info("Waiting for oop-browser-crashed event.");

  Services.telemetry.clearScalars();
  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );

  let eventPromise = getEventPromise("oop-browser-crashed", "basic", kTimeout);
  await openNewTab(true);
  await eventPromise;

  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );
});
