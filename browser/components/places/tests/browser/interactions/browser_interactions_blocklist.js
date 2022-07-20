/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that interactions are not recorded for sites on the blocklist.
 */

const ALLOWED_TEST_URL = "http://mochi.test:8888/";
const BLOCKED_TEST_URL = "https://example.com/browser";

ChromeUtils.defineESModuleGetters(this, {
  InteractionsBlocklist: "resource:///modules/InteractionsBlocklist.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  FilterAdult: "resource://activity-stream/lib/FilterAdult.jsm",
});

add_setup(async function() {
  let oldBlocklistValue = Services.prefs.getStringPref(
    "places.interactions.customBlocklist",
    "[]"
  );
  Services.prefs.setStringPref("places.interactions.customBlocklist", "[]");

  registerCleanupFunction(async () => {
    Services.prefs.setStringPref(
      "places.interactions.customBlocklist",
      oldBlocklistValue
    );
  });
});
/**
 * Loads the blocked URL, then loads about:blank to trigger the end of the
 * interaction with the blocked URL.
 *
 * @param {boolean} expectRecording
 *   True if we expect the blocked URL to have been recorded in the database.
 */
async function loadBlockedUrl(expectRecording) {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(ALLOWED_TEST_URL, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    BrowserTestUtils.loadURI(browser, BLOCKED_TEST_URL);
    await BrowserTestUtils.browserLoaded(browser, false, BLOCKED_TEST_URL);

    await assertDatabaseValues([
      {
        url: ALLOWED_TEST_URL,
        totalViewTime: 10000,
      },
    ]);

    Interactions._pageViewStartTime = Cu.now() - 20000;

    BrowserTestUtils.loadURI(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    if (expectRecording) {
      await assertDatabaseValues([
        {
          url: ALLOWED_TEST_URL,
          totalViewTime: 10000,
        },
        {
          url: BLOCKED_TEST_URL,
          totalViewTime: 20000,
        },
      ]);
    } else {
      await assertDatabaseValues([
        {
          url: ALLOWED_TEST_URL,
          totalViewTime: 10000,
        },
      ]);
    }
  });
}

add_task(async function test_regexp() {
  info("Record BLOCKED_TEST_URL because it is not yet blocklisted.");
  await loadBlockedUrl(true);

  info("Add BLOCKED_TEST_URL to the blocklist and verify it is not recorded.");
  let blockedRegex = /^(https?:\/\/)?example\.com\/browser/i;
  InteractionsBlocklist.addRegexToBlocklist(blockedRegex);
  Assert.equal(
    Services.prefs.getStringPref("places.interactions.customBlocklist", "[]"),
    JSON.stringify([blockedRegex.toString()])
  );
  await loadBlockedUrl(false);

  info("Remove BLOCKED_TEST_URL from the blocklist and verify it is recorded.");
  InteractionsBlocklist.removeRegexFromBlocklist(blockedRegex);
  Assert.equal(
    Services.prefs.getStringPref("places.interactions.customBlocklist", "[]"),
    JSON.stringify([])
  );
  await loadBlockedUrl(true);
});

add_task(async function test_adult() {
  FilterAdult.addDomainToList("https://example.com/browser");
  await loadBlockedUrl(false);
  FilterAdult.removeDomainFromList("https://example.com/browser");
});
