/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that interactions are not recorded for sites on the blocklist.
 */

const ALLOWED_TEST_URL = "https://example.com/";
const BLOCKED_TEST_URL = "https://example.com/browser";

add_task(async function setup() {
  sinon.spy(Interactions, "_updateDatabase");

  registerCleanupFunction(() => {
    sinon.restore();
  });
});

add_task(async function test() {
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

    // We should not have updated the database with BLOCKED_TEST_URL because it
    // is blocklisted. We wait a little to make sure _updateDatabase is not
    // going to fire.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 500));
    await assertDatabaseValues([
      {
        url: ALLOWED_TEST_URL,
        totalViewTime: 10000,
      },
    ]);
  });
});
