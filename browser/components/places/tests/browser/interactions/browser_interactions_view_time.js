/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests page view time recording for interactions.
 */

const TEST_URL = "https://example.com/";
const TEST_URL2 = "https://example.com/browser";

add_task(async function setup() {
  sinon.spy(Interactions, "_updateDatabase");

  registerCleanupFunction(() => {
    sinon.restore();
  });
});

async function assertDatabaseValues(args, expected) {
  await BrowserTestUtils.waitForCondition(
    () => Interactions._updateDatabase.callCount == expected.length,
    "Should have saved to the database"
  );

  for (let i = 0; i < expected.length; i++) {
    let actual = args[i][0];
    Assert.equal(
      actual.url,
      expected[i].url,
      "Should have saved the page into the database"
    );
    if (expected[i].exactTotalViewTime) {
      Assert.equal(
        actual.totalViewTime,
        expected[i].exactTotalViewTime,
        "Should have kept the exact time."
      );
    } else {
      Assert.greater(
        actual.totalViewTime,
        expected[i].totalViewTime,
        "Should have stored the interaction time."
      );
    }
  }
}

add_task(async function test_interactions_simple_load_and_navigate_away() {
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._lastUpdateTime = Cu.now() - 10000;

    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    await assertDatabaseValues(Interactions._updateDatabase.args, [
      {
        url: TEST_URL,
        totalViewTime: 10000,
      },
    ]);

    Interactions._lastUpdateTime = Cu.now() - 20000;

    BrowserTestUtils.loadURI(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues(Interactions._updateDatabase.args, [
      {
        url: TEST_URL,
        totalViewTime: 10000,
      },
      {
        url: TEST_URL2,
        totalViewTime: 20000,
      },
    ]);
  });
});

add_task(async function test_interactions_close_tab() {
  sinon.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._lastUpdateTime = Cu.now() - 20000;
  });

  await assertDatabaseValues(Interactions._updateDatabase.args, [
    {
      url: TEST_URL,
      totalViewTime: 20000,
    },
  ]);
});

add_task(async function test_interactions_background_tab() {
  sinon.reset();
  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  let tab2 = BrowserTestUtils.addTab(gBrowser, TEST_URL2);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, TEST_URL2);

  Interactions._lastUpdateTime = Cu.now() - 10000;

  BrowserTestUtils.removeTab(tab2);

  // This is checking a non-action, so let the event queue clear to try and
  // detect any unexpected database writes.
  await TestUtils.waitForTick();

  Assert.equal(
    Interactions._updateDatabase.callCount,
    0,
    "Should not have recorded any interactions."
  );

  BrowserTestUtils.removeTab(tab1);

  // Only the interaction in the visible tab should have been recorded.
  await assertDatabaseValues(Interactions._updateDatabase.args, [
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
  ]);
});

add_task(async function test_interactions_switch_tabs() {
  sinon.reset();
  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  let tab2 = BrowserTestUtils.addTab(gBrowser, TEST_URL2);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, TEST_URL2);

  info("Switch to second tab");
  Interactions._lastUpdateTime = Cu.now() - 10000;
  gBrowser.selectedTab = tab2;

  // Only the interaction of the first tab should be recorded so far.
  await assertDatabaseValues(Interactions._updateDatabase.args, [
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
  ]);
  let tab1ViewTime = Interactions._updateDatabase.args[0][0].totalViewTime;

  info("Switch back to first tab");
  Interactions._lastUpdateTime = Cu.now() - 20000;
  gBrowser.selectedTab = tab1;

  // The interaction of the second tab should now be recorded.
  await assertDatabaseValues(Interactions._updateDatabase.args, [
    {
      url: TEST_URL,
      exactTotalViewTime: tab1ViewTime,
    },
    {
      url: TEST_URL2,
      totalViewTime: 20000,
    },
  ]);

  info("Switch to second tab again");
  // We reset the stub here to make the database change clearer.
  sinon.reset();
  Interactions._lastUpdateTime = Cu.now() - 30000;
  gBrowser.selectedTab = tab2;

  // The interaction of the second tab should now be recorded.
  await assertDatabaseValues(Interactions._updateDatabase.args, [
    {
      url: TEST_URL,
      totalViewTime: tab1ViewTime + 30000,
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
