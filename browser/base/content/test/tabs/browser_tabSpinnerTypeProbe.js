"use strict";

// Keep this in sync with the order in Histograms.json for
// FX_TAB_SWITCH_SPINNER_TYPE
const CATEGORIES = [
  "seen",
  "unseenOld",
  "unseenNew",
];

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // We can interrupt JS to paint now, which is great for
      // users, but bad for testing spinners. We temporarily
      // disable that feature for this test so that we can
      // easily get ourselves into a predictable tab spinner
      // state.
      ["browser.tabs.remote.force-paint", false],
    ]
  });
});

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
 * Takes a Telemetry histogram snapshot and makes sure
 * that the index for that value (as defined by CATEGORIES)
 * has a count of 1, and that it's the only value that
 * has been incremented.
 *
 * @param snapshot (Object)
 *        The Telemetry histogram snapshot to examine.
 * @param category (String)
 *        The category in CATEGORIES whose index we expect to have
 *        been set to 1.
 */
function assertOnlyOneTypeSet(snapshot, category) {
  let categoryIndex = CATEGORIES.indexOf(category);
  Assert.equal(snapshot.counts[categoryIndex], 1,
               `Should have seen the ${category} count increment.`);
  // Use Array.prototype.reduce to sum up all of the
  // snapshot.count entries
  Assert.equal(snapshot.counts.reduce((a, b) => a + b), 1,
               "Should only be 1 collected value.");
}

Assert.ok(gMultiProcessBrowser,
  "These tests only makes sense in an e10s-enabled window.");

let gHistogram = Services.telemetry
                         .getHistogramById("FX_TAB_SWITCH_SPINNER_TYPE");

/**
 * This test tests that the "seen" category for the FX_TAB_SWITCH_SPINNER_TYPE
 * probe works. This means that we show a spinner for a tab that we've
 * presented before.
 */
add_task(async function test_seen_spinner_type_probe() {
  let originalTab = gBrowser.selectedTab;

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, async function(browser) {
    // We'll switch away from the current tab, then hang it, and then switch
    // back to it. This should add to the "seen" type for the histogram.
    let testTab = gBrowser.selectedTab;
    await BrowserTestUtils.switchTab(gBrowser, originalTab);
    gHistogram.clear();

    let tabHangPromise = hangContentProcess(browser, 1000);
    let hangTabSwitch = BrowserTestUtils.switchTab(gBrowser, testTab);
    await tabHangPromise;
    await hangTabSwitch;

    // Okay, we should have gotten an entry in our Histogram for that one.
    let snapshot = gHistogram.snapshot();
    assertOnlyOneTypeSet(snapshot, "seen");
    gHistogram.clear();
  });
});

/**
 * This test tests that the "unseenOld" category for the FX_TAB_SWITCH_SPINNER_TYPE
 * probe works. This means that we show a spinner for a tab that we've never
 * seen before, and enough time has passed since its creation that we consider
 * it "old" (See the NEWNESS_THRESHOLD constant in the async tabswitcher for
 * the exact definition).
 */
add_task(async function test_unseenOld_spinner_type_probe() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, async function(browser) {
    const NEWNESS_THRESHOLD = gBrowser._getSwitcher().NEWNESS_THRESHOLD;

    // First, create a new background tab, ensuring that it's in the same process
    // as the current one.
    let bgTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      sameProcessAsFrameLoader: browser.frameLoader,
      inBackground: true,
    });

    await BrowserTestUtils.browserLoaded(bgTab.linkedBrowser);

    // Now, let's fudge with the creationTime of the background tab so that
    // it seems old. We'll also add a fudge-factor to the NEWNESS_THRESHOLD of 100ms
    // to try to avoid any potential timing issues.
    bgTab._creationTime = bgTab._creationTime - NEWNESS_THRESHOLD - 100;

    // Okay, tab should seem sufficiently old now so that a spinner in it should
    // qualify for "unseenOld". Let's hang it and switch to it.
    gHistogram.clear();
    let tabHangPromise = hangContentProcess(browser, 1000);
    let hangTabSwitch = BrowserTestUtils.switchTab(gBrowser, bgTab);
    await tabHangPromise;
    await hangTabSwitch;

    // Okay, we should have gotten an entry in our Histogram for that one.
    let snapshot = gHistogram.snapshot();
    assertOnlyOneTypeSet(snapshot, "unseenOld");

    await BrowserTestUtils.removeTab(bgTab);
  });
});

/**
 * This test tests that the "unseenNew" category for the FX_TAB_SWITCH_SPINNER_TYPE
 * probe works. This means that we show a spinner for a tab that we've never
 * seen before, and not enough time has passed since its creation that we consider
 * it "old" (See the NEWNESS_THRESHOLD constant in the async tabswitcher for
 * the exact definition).
 */
add_task(async function test_unseenNew_spinner_type_probe() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, async function(browser) {
    // First, create a new background tab, ensuring that it's in the same process
    // as the current one.
    let bgTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      sameProcessAsFrameLoader: browser.frameLoader,
      inBackground: true,
    });

    await BrowserTestUtils.browserLoaded(bgTab.linkedBrowser);

    // Now, let's fudge with the creationTime of the background tab so that
    // it seems very new (created 1 minute into the future).
    bgTab._creationTime = Date.now() + (60 * 1000);

    // Okay, tab should seem sufficiently new now so that a spinner in it should
    // qualify for "unseenNew". Let's hang it and switch to it.
    gHistogram.clear();
    let tabHangPromise = hangContentProcess(browser, 1000);
    let hangTabSwitch = BrowserTestUtils.switchTab(gBrowser, bgTab);
    await tabHangPromise;
    await hangTabSwitch;

    // Okay, we should have gotten an entry in our Histogram for that one.
    let snapshot = gHistogram.snapshot();
    assertOnlyOneTypeSet(snapshot, "unseenNew");

    await BrowserTestUtils.removeTab(bgTab);
  });
});
