/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that we record hangs in the parent process in telemetry events.
 * This test would be an xpcshell test except xpcshell does not think
 * it is running e10s (see bug 1568333).
 */
add_task(async function test_browser_hang() {
  // Trip some testing code to ensure we can test this. Sadly, this is a magic
  // number corresponding to code in XPCJSContext.cpp
  await SpecialPowers.pushPrefEnv({
    set: [["dom.max_chrome_script_run_time", 2]],
  });
  await SpecialPowers.promiseTimeout(0);

  // Hang for 1.2 seconds.
  let now = Date.now();
  let i = 0;
  info("Start loop");
  while (Date.now() - now < 2500) {
    // The system clock can go backwards. Don't time out the test:
    if (Date.now() - now < 0) {
      info("Yikes, the system clock changed while running this test.");
      now = Date.now();
    }
    i++;
  }
  let duration = (Date.now() - now) / 1000;
  info("Looped " + i + " iterations.");

  let events;
  await TestUtils.waitForCondition(() => {
    events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_ALL_CHANNELS,
      false
    );
    return events.parent?.some(e => e[1] == "slow_script_warning");
  }, "Should find an event after doing this.").catch(e => ok(false, e));
  events = events.parent || [];
  let event = events.find(e => e[1] == "slow_script_warning");
  ok(event, "Should have registered an event.");
  if (event) {
    is(event[3], "browser", "Should register as browser hang.");
    let args = event[5];
    is(args.uri_type, "browser", "Should register browser uri type.");
    Assert.greater(
      duration + 1,
      parseFloat(args.hang_duration),
      "hang duration should not exaggerate."
    );
    Assert.less(
      duration - 1,
      parseFloat(args.hang_duration),
      "hang duration should not undersell."
    );
  }
});
