"use strict";

var gAnimHistogram = Services.telemetry
                             .getHistogramById("FX_TAB_CLOSE_TIME_ANIM_MS");
var gNoAnimHistogram = Services.telemetry
                               .getHistogramById("FX_TAB_CLOSE_TIME_NO_ANIM_MS");

/**
 * Takes a Telemetry histogram snapshot and makes sure
 * that the sum of all counts equals expectedCount.
 *
 * @param snapshot (Object)
 *        The Telemetry histogram snapshot to examine.
 * @param expectedCount (int)
 *        What we expect the number of incremented counts to be. For example,
 *        If we expect this probe to have only had a single recording, this
 *        would be 1. If we expected it to have not recorded any data at all,
 *        this would be 0.
 */
function assertCount(snapshot, expectedCount) {
  // Use Array.prototype.reduce to sum up all of the
  // snapshot.count entries
  Assert.equal(snapshot.counts.reduce((a, b) => a + b), expectedCount,
               `Should only be ${expectedCount} collected value.`);
}

add_task(async function setup() {
  // These probes are opt-in, meaning we only capture them if extended
  // Telemetry recording is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["toolkit.telemetry.enabled", true]]
  });

  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

/**
 * Tests the FX_TAB_CLOSE_TIME_ANIM_MS probe by closing a tab with the tab
 * close animation.
 */
add_task(async function test_close_time_anim_probe() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.waitForCondition(() => tab._fullyOpen);

  gAnimHistogram.clear();
  gNoAnimHistogram.clear();

  await BrowserTestUtils.removeTab(tab, { animate: true });

  assertCount(gAnimHistogram.snapshot(), 1);
  assertCount(gNoAnimHistogram.snapshot(), 0);

  gAnimHistogram.clear();
  gNoAnimHistogram.clear();
});

/**
 * Tests the FX_TAB_CLOSE_TIME_NO_ANIM_MS probe by closing a tab without the
 * tab close animation.
 */
add_task(async function test_close_time_no_anim_probe() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.waitForCondition(() => tab._fullyOpen);

  gAnimHistogram.clear();
  gNoAnimHistogram.clear();

  await BrowserTestUtils.removeTab(tab, { animate: false });

  assertCount(gAnimHistogram.snapshot(), 0);
  assertCount(gNoAnimHistogram.snapshot(), 1);

  gAnimHistogram.clear();
  gNoAnimHistogram.clear();
});
