"use strict";

/**
 * Tests the FX_TAB_SWITCH_SPINNER_VISIBLE_MS and
 * FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS telemetry probes
 */
const MIN_HANG_TIME = 500; // ms
const MAX_HANG_TIME = 5 * 1000; // ms

/**
 * Returns the sum of all values in an array.
 * @param  {Array}  aArray An array of integers
 * @return {Number} The sum of the integers in the array
 */
function sum(aArray) {
  return aArray.reduce(function(previousValue, currentValue) {
    return previousValue + currentValue;
  });
}

/**
 * Causes the content process for a remote <xul:browser> to run
 * some busy JS for aMs milliseconds.
 *
 * @param {<xul:browser>} browser
 *        The browser that's running in the content process that we're
 *        going to hang.
 * @param {int} aMs
 *        The amount of time, in milliseconds, to hang the content process.
 *
 * @return {Promise}
 *        Resolves once the hang is done.
 */
function hangContentProcess(browser, aMs) {
  return ContentTask.spawn(browser, aMs, async function(ms) {
    let then = Date.now();
    while (Date.now() - then < ms) {
      // Let's burn some CPU...
    }
  });
}

/**
 * A generator intended to be run as a Task. It tests one of the tab spinner
 * telemetry probes.
 * @param {String} aProbe The probe to test. Should be one of:
 *                  - FX_TAB_SWITCH_SPINNER_VISIBLE_MS
 *                  - FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS
 */
async function testProbe(aProbe) {
  info(`Testing probe: ${aProbe}`);
  let histogram = Services.telemetry.getHistogramById(aProbe);
  let buckets = histogram.snapshot().ranges.filter(function(value) {
    return (value > MIN_HANG_TIME && value < MAX_HANG_TIME);
  });
  let delayTime = buckets[0]; // Pick a bucket arbitrarily

  // The tab spinner does not show up instantly. We need to hang for a little
  // bit of extra time to account for the tab spinner delay.
  delayTime += gBrowser.selectedTab.linkedBrowser.getTabBrowser()._getSwitcher().TAB_SWITCH_TIMEOUT;

  // In order for a spinner to be shown, the tab must have presented before.
  let origTab = gBrowser.selectedTab;
  let hangTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let hangBrowser = hangTab.linkedBrowser;
  ok(hangBrowser.isRemoteBrowser, "New tab should be remote.");
  ok(hangBrowser.frameLoader.tabParent.hasPresented, "New tab has presented.");

  // Now switch back to the original tab and set up our hang.
  await BrowserTestUtils.switchTab(gBrowser, origTab);

  let tabHangPromise = hangContentProcess(hangBrowser, delayTime);
  histogram.clear();
  let hangTabSwitch = BrowserTestUtils.switchTab(gBrowser, hangTab);
  await tabHangPromise;
  await hangTabSwitch;

  // Now we should have a hang in our histogram.
  let snapshot = histogram.snapshot();
  await BrowserTestUtils.removeTab(hangTab);
  ok(sum(snapshot.counts) > 0,
   `Spinner probe should now have a value in some bucket`);
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processCount", 1],
      // We can interrupt JS to paint now, which is great for
      // users, but bad for testing spinners. We temporarily
      // disable that feature for this test so that we can
      // easily get ourselves into a predictable tab spinner
      // state.
      ["browser.tabs.remote.force-paint", false],
    ]
  });
});

add_task(testProbe.bind(null, "FX_TAB_SWITCH_SPINNER_VISIBLE_MS"));
add_task(testProbe.bind(null, "FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS"));
