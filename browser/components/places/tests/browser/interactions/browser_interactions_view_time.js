/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests page view time recording for interactions.
 */

const TEST_URL = "https://example.com/";
const TEST_URL2 = "https://example.com/browser";
const TEST_URL3 = "https://example.com/browser/browser";
const TEST_URL4 = "https://example.com/browser/browser/components";

add_task(async function setup() {
  sinon.spy(Interactions, "_updateDatabase");

  registerCleanupFunction(() => {
    sinon.restore();
  });
});

async function assertDatabaseValues(expected) {
  await BrowserTestUtils.waitForCondition(
    () => Interactions._updateDatabase.callCount == expected.length,
    "Should have saved to the database"
  );

  let args = Interactions._updateDatabase.args;
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
    Interactions._pageViewStartTime = Cu.now() - 10000;

    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    await assertDatabaseValues([
      {
        url: TEST_URL,
        totalViewTime: 10000,
      },
    ]);

    Interactions._pageViewStartTime = Cu.now() - 20000;

    BrowserTestUtils.loadURI(browser, "about:blank");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

    await assertDatabaseValues([
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
    Interactions._pageViewStartTime = Cu.now() - 20000;
  });

  await assertDatabaseValues([
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

  Interactions._pageViewStartTime = Cu.now() - 10000;

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
  await assertDatabaseValues([
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
  Interactions._pageViewStartTime = Cu.now() - 10000;
  gBrowser.selectedTab = tab2;

  // Only the interaction of the first tab should be recorded so far.
  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
  ]);
  let tab1ViewTime = Interactions._updateDatabase.args[0][0].totalViewTime;

  info("Switch back to first tab");
  Interactions._pageViewStartTime = Cu.now() - 20000;
  gBrowser.selectedTab = tab1;

  // The interaction of the second tab should now be recorded.
  await assertDatabaseValues([
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
  Interactions._pageViewStartTime = Cu.now() - 30000;
  gBrowser.selectedTab = tab2;

  // The interaction of the second tab should now be recorded.
  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: tab1ViewTime + 30000,
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function test_interactions_switch_windows() {
  sinon.reset();

  // Open a tab in the first window.
  let tabInOriginalWindow = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  // and then load the second window.
  Interactions._pageViewStartTime = Cu.now() - 10000;

  let otherWin = await BrowserTestUtils.openNewBrowserWindow();

  BrowserTestUtils.loadURI(otherWin.gBrowser.selectedBrowser, TEST_URL2);
  await BrowserTestUtils.browserLoaded(
    otherWin.gBrowser.selectedBrowser,
    false,
    TEST_URL2
  );

  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
  ]);
  let originalWindowViewTime =
    Interactions._updateDatabase.args[0][0].totalViewTime;

  info("Switch back to original window");
  Interactions._pageViewStartTime = Cu.now() - 20000;
  window.focus();

  await assertDatabaseValues([
    {
      url: TEST_URL,
      exactTotalViewTime: originalWindowViewTime,
    },
    {
      url: TEST_URL2,
      totalViewTime: 20000,
    },
  ]);
  let newWindowViewTime = Interactions._updateDatabase.args[1][0].totalViewTime;

  info("Switch back to new window");
  Interactions._pageViewStartTime = Cu.now() - 30000;
  otherWin.focus();

  await assertDatabaseValues([
    {
      url: TEST_URL,
      // Note: this should be `exactTotalViewTime:originalWindowViewTime`, but
      // apply updates to the same object.
      totalViewTime: originalWindowViewTime + 30000,
    },
    {
      url: TEST_URL2,
      exactTotalViewTime: newWindowViewTime,
    },
    {
      url: TEST_URL,
      totalViewTime: originalWindowViewTime + 30000,
    },
  ]);

  BrowserTestUtils.removeTab(tabInOriginalWindow);
  await BrowserTestUtils.closeWindow(otherWin);
});

add_task(async function test_interactions_loading_in_unfocused_windows() {
  sinon.reset();

  let otherWin = await BrowserTestUtils.openNewBrowserWindow();

  BrowserTestUtils.loadURI(otherWin.gBrowser.selectedBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(
    otherWin.gBrowser.selectedBrowser,
    false,
    TEST_URL
  );

  Interactions._pageViewStartTime = Cu.now() - 10000;

  BrowserTestUtils.loadURI(otherWin.gBrowser.selectedBrowser, TEST_URL2);
  await BrowserTestUtils.browserLoaded(
    otherWin.gBrowser.selectedBrowser,
    false,
    TEST_URL2
  );

  // Only the interaction of the first tab should be recorded so far.
  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
  ]);
  let newWindowViewTime = Interactions._updateDatabase.args[0][0].totalViewTime;

  // Open a tab in the background window, and then navigate somewhere else,
  // this should not record an intereaction.
  let tabInOriginalWindow = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL3,
  });

  Interactions._pageViewStartTime = Cu.now() - 20000;

  BrowserTestUtils.loadURI(tabInOriginalWindow.linkedBrowser, TEST_URL4);
  await BrowserTestUtils.browserLoaded(
    tabInOriginalWindow.linkedBrowser,
    false,
    TEST_URL4
  );

  // Only the interaction of the first tab should be recorded so far.
  await assertDatabaseValues([
    {
      url: TEST_URL,
      exactTotalViewTime: newWindowViewTime,
    },
  ]);

  BrowserTestUtils.removeTab(tabInOriginalWindow);
  await BrowserTestUtils.closeWindow(otherWin);
});

add_task(async function test_interactions_private_browsing() {
  sinon.reset();

  // Open a tab in the first window.
  let tabInOriginalWindow = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  // and then load the second window.
  Interactions._pageViewStartTime = Cu.now() - 10000;

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  BrowserTestUtils.loadURI(privateWin.gBrowser.selectedBrowser, TEST_URL2);
  await BrowserTestUtils.browserLoaded(
    privateWin.gBrowser.selectedBrowser,
    false,
    TEST_URL2
  );

  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
  ]);
  let originalWindowViewTime =
    Interactions._updateDatabase.args[0][0].totalViewTime;

  info("Switch back to original window");
  Interactions._pageViewStartTime = Cu.now() - 20000;
  window.focus();
  // As we're checking for a non-action, wait for the focus to have definitely
  // completed, and then let the event queues clear.
  await SimpleTest.promiseFocus(window);
  await TestUtils.waitForTick();

  // The private window site should not be recorded.
  await assertDatabaseValues([
    {
      url: TEST_URL,
      exactTotalViewTime: originalWindowViewTime,
    },
  ]);

  info("Switch back to new window");
  Interactions._pageViewStartTime = Cu.now() - 30000;
  privateWin.focus();

  await assertDatabaseValues([
    {
      url: TEST_URL,
      // Note: this should be `exactTotalViewTime:originalWindowViewTime`, but
      // apply updates to the same object.
      totalViewTime: originalWindowViewTime + 30000,
    },
    {
      url: TEST_URL,
      totalViewTime: originalWindowViewTime + 30000,
    },
  ]);

  BrowserTestUtils.removeTab(tabInOriginalWindow);
  await BrowserTestUtils.closeWindow(privateWin);
});
