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

add_task(async function test_interactions_simple_load_and_navigate_away() {
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    BrowserTestUtils.startLoadingURIString(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);

    await assertDatabaseValues([
      {
        url: TEST_URL,
        totalViewTime: 10000,
      },
    ]);

    Interactions._pageViewStartTime = Cu.now() - 20000;

    BrowserTestUtils.startLoadingURIString(browser, "about:blank");
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

add_task(async function test_interactions_simple_load_and_change_to_non_http() {
  await Interactions.reset();
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    BrowserTestUtils.startLoadingURIString(browser, "about:support");
    await BrowserTestUtils.browserLoaded(browser, false, "about:support");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        totalViewTime: 10000,
      },
    ]);
  });
});

add_task(async function test_interactions_close_tab() {
  await Interactions.reset();
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
  await Interactions.reset();
  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  let tab2 = BrowserTestUtils.addTab(gBrowser, TEST_URL2);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, TEST_URL2);

  Interactions._pageViewStartTime = Cu.now() - 10000;

  BrowserTestUtils.removeTab(tab2);

  // This is checking a non-action, so let the event queue clear to try and
  // detect any unexpected database writes. We wait for a few ticks to
  // make it more likely. however if this fails it may show up as an
  // intermittent.
  await TestUtils.waitForTick();
  await TestUtils.waitForTick();
  await TestUtils.waitForTick();

  await assertDatabaseValues([]);

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
  await Interactions.reset();
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
  let tab1ViewTime = await getDatabaseValue(TEST_URL, "totalViewTime");

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
  Interactions._pageViewStartTime = Cu.now() - 30000;
  gBrowser.selectedTab = tab2;

  // The interaction of the second tab should now be recorded.
  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: tab1ViewTime + 30000,
    },
    {
      url: TEST_URL2,
      totalViewTime: 20000,
    },
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function test_interactions_switch_windows() {
  await Interactions.reset();

  // Open a tab in the first window.
  let tabInOriginalWindow = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  // and then load the second window.
  Interactions._pageViewStartTime = Cu.now() - 10000;

  let otherWin = await BrowserTestUtils.openNewBrowserWindow();

  BrowserTestUtils.startLoadingURIString(
    otherWin.gBrowser.selectedBrowser,
    TEST_URL2
  );
  await BrowserTestUtils.browserLoaded(
    otherWin.gBrowser.selectedBrowser,
    false,
    TEST_URL2
  );
  await SimpleTest.promiseFocus(otherWin);
  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
  ]);
  let originalWindowViewTime = await getDatabaseValue(
    TEST_URL,
    "totalViewTime"
  );

  info("Switch back to original window");
  Interactions._pageViewStartTime = Cu.now() - 20000;
  await SimpleTest.promiseFocus(window);
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
  let newWindowViewTime = await getDatabaseValue(TEST_URL2, "totalViewTime");

  info("Switch back to new window");
  Interactions._pageViewStartTime = Cu.now() - 30000;
  await SimpleTest.promiseFocus(otherWin);
  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: originalWindowViewTime + 30000,
    },
    {
      url: TEST_URL2,
      exactTotalViewTime: newWindowViewTime,
    },
  ]);

  BrowserTestUtils.removeTab(tabInOriginalWindow);
  await BrowserTestUtils.closeWindow(otherWin);
});

add_task(async function test_interactions_loading_in_unfocused_windows() {
  await Interactions.reset();

  let otherWin = await BrowserTestUtils.openNewBrowserWindow();

  BrowserTestUtils.startLoadingURIString(
    otherWin.gBrowser.selectedBrowser,
    TEST_URL
  );
  await BrowserTestUtils.browserLoaded(
    otherWin.gBrowser.selectedBrowser,
    false,
    TEST_URL
  );

  Interactions._pageViewStartTime = Cu.now() - 10000;

  BrowserTestUtils.startLoadingURIString(
    otherWin.gBrowser.selectedBrowser,
    TEST_URL2
  );
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
  let newWindowViewTime = await getDatabaseValue(TEST_URL, "totalViewTime");

  // Open a tab in the background window, and then navigate somewhere else,
  // this should not record an intereaction.
  let tabInOriginalWindow = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL3,
  });

  Interactions._pageViewStartTime = Cu.now() - 20000;

  BrowserTestUtils.startLoadingURIString(
    tabInOriginalWindow.linkedBrowser,
    TEST_URL4
  );
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
  await Interactions.reset();

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

  BrowserTestUtils.startLoadingURIString(
    privateWin.gBrowser.selectedBrowser,
    TEST_URL2
  );
  await BrowserTestUtils.browserLoaded(
    privateWin.gBrowser.selectedBrowser,
    false,
    TEST_URL2
  );
  await SimpleTest.promiseFocus(privateWin);
  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: 10000,
    },
  ]);
  let originalWindowViewTime = await getDatabaseValue(
    TEST_URL,
    "totalViewTime"
  );

  info("Switch back to original window");
  Interactions._pageViewStartTime = Cu.now() - 20000;
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
  await SimpleTest.promiseFocus(privateWin);
  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: originalWindowViewTime + 30000,
    },
  ]);

  BrowserTestUtils.removeTab(tabInOriginalWindow);
  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function test_interactions_idle() {
  await Interactions.reset();
  let lastViewTime;

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    Interactions.observe(null, "idle", "");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        totalViewTime: 10000,
      },
    ]);
    lastViewTime = await getDatabaseValue(TEST_URL, "totalViewTime");

    Interactions._pageViewStartTime = Cu.now() - 20000;

    Interactions.observe(null, "active", "");

    await assertDatabaseValues([
      {
        url: TEST_URL,
        exactTotalViewTime: lastViewTime,
      },
    ]);

    Interactions._pageViewStartTime = Cu.now() - 30000;
  });

  await assertDatabaseValues([
    {
      url: TEST_URL,
      totalViewTime: lastViewTime + 30000,
      maxViewTime: lastViewTime + 30000 + 10000,
    },
  ]);
});
