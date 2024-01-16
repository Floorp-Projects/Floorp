/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(globalThis, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});
const { ProfileAge } = ChromeUtils.importESModule(
  "resource://gre/modules/ProfileAge.sys.mjs"
);

const HAS_IMPORTED_HISTORY_PREF = "browser.migrate.interactions.history";
const IMPORT_HISTORY_DISMISSED_PREF =
  "browser.tabs.firefox-view.importHistory.dismissed";
const HISTORY_EVENT = [["firefoxview_next", "history", "visits", undefined]];
const SHOW_ALL_HISTORY_EVENT = [
  ["firefoxview_next", "show_all_history", "tabs", undefined],
];

const NEVER_REMEMBER_HISTORY_PREF = "browser.privatebrowsing.autostart";
const DAY_MS = 24 * 60 * 60 * 1000;
const today = new Date();
const yesterday = new Date(Date.now() - DAY_MS);
const twoDaysAgo = new Date(Date.now() - DAY_MS * 2);
const threeDaysAgo = new Date(Date.now() - DAY_MS * 3);
const fourDaysAgo = new Date(Date.now() - DAY_MS * 4);
const oneMonthAgo = new Date(today);

// Set the date for the first day of the last month
oneMonthAgo.setDate(1);
if (oneMonthAgo.getMonth() === 0) {
  // If today's date is in January, use first day in December from the previous year
  oneMonthAgo.setMonth(11);
  oneMonthAgo.setFullYear(oneMonthAgo.getFullYear() - 1);
} else {
  oneMonthAgo.setMonth(oneMonthAgo.getMonth() - 1);
}

function isElInViewport(element) {
  const boundingRect = element.getBoundingClientRect();
  return (
    boundingRect.top >= 0 &&
    boundingRect.left >= 0 &&
    boundingRect.bottom <=
      (window.innerHeight || document.documentElement.clientHeight) &&
    boundingRect.right <=
      (window.innerWidth || document.documentElement.clientWidth)
  );
}

async function historyComponentReady(historyComponent) {
  await TestUtils.waitForCondition(
    () =>
      [...historyComponent.allHistoryItems.values()].reduce(
        (acc, { length }) => acc + length,
        0
      ) === 24
  );

  let expected = historyComponent.historyMapByDate.length;
  let actual = historyComponent.cards.length;

  is(expected, actual, `Total number of cards should be ${expected}`);
}

async function historyTelemetry() {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for history firefoxview telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    HISTORY_EVENT,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

async function sortHistoryTelemetry(sortHistoryEvent) {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for sort_history firefoxview telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    sortHistoryEvent,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

async function showAllHistoryTelemetry() {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for show_all_history firefoxview telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    SHOW_ALL_HISTORY_EVENT,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

async function addHistoryItems(dateAdded) {
  await PlacesUtils.history.insert({
    url: URLs[0],
    title: "Example Domain 1",
    visits: [{ date: dateAdded }],
  });
  await PlacesUtils.history.insert({
    url: URLs[1],
    title: "Example Domain 2",
    visits: [{ date: dateAdded }],
  });
  await PlacesUtils.history.insert({
    url: URLs[2],
    title: "Example Domain 3",
    visits: [{ date: dateAdded }],
  });
  await PlacesUtils.history.insert({
    url: URLs[3],
    title: "Example Domain 4",
    visits: [{ date: dateAdded }],
  });
}

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.search.enabled", true]],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_list_ordering() {
  await PlacesUtils.history.clear();
  await addHistoryItems(today);
  await addHistoryItems(yesterday);
  await addHistoryItems(twoDaysAgo);
  await addHistoryItems(threeDaysAgo);
  await addHistoryItems(fourDaysAgo);
  await addHistoryItems(oneMonthAgo);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    await navigateToCategoryAndWait(document, "history");

    let historyComponent = document.querySelector("view-history");
    historyComponent.profileAge = 8;

    await historyComponentReady(historyComponent);

    let firstCard = historyComponent.cards[0];

    info("The first card should have a header for 'Today'.");
    await BrowserTestUtils.waitForMutationCondition(
      firstCard.querySelector("[slot=header]"),
      { attributes: true },
      () =>
        document.l10n.getAttributes(firstCard.querySelector("[slot=header]"))
          .id === "firefoxview-history-date-today"
    );

    // Select first history item in first card
    await clearAllParentTelemetryEvents();
    await TestUtils.waitForCondition(() => {
      return historyComponent.lists[0].rowEls.length;
    });
    let firstHistoryLink = historyComponent.lists[0].rowEls[0].mainEl;
    let promiseHidden = BrowserTestUtils.waitForEvent(
      document,
      "visibilitychange"
    );
    await EventUtils.synthesizeMouseAtCenter(firstHistoryLink, {}, content);
    await historyTelemetry();
    await promiseHidden;
    await openFirefoxViewTab(browser.ownerGlobal);

    // Test number of cards when sorted by site/domain
    await clearAllParentTelemetryEvents();
    let sortHistoryEvent = [
      [
        "firefoxview_next",
        "sort_history",
        "tabs",
        undefined,
        { sort_type: "site" },
      ],
    ];
    // Select sort by site option
    await EventUtils.synthesizeMouseAtCenter(
      historyComponent.sortInputs[1],
      {},
      content
    );
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);
    await sortHistoryTelemetry(sortHistoryEvent);

    let expectedNumOfCards = historyComponent.historyMapBySite.length;

    info(`Total number of cards should be ${expectedNumOfCards}`);
    await BrowserTestUtils.waitForMutationCondition(
      historyComponent.shadowRoot,
      { childList: true, subtree: true },
      () => expectedNumOfCards === historyComponent.cards.length
    );

    await clearAllParentTelemetryEvents();
    sortHistoryEvent = [
      [
        "firefoxview_next",
        "sort_history",
        "tabs",
        undefined,
        { sort_type: "date" },
      ],
    ];
    // Select sort by date option
    await EventUtils.synthesizeMouseAtCenter(
      historyComponent.sortInputs[0],
      {},
      content
    );
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);
    await sortHistoryTelemetry(sortHistoryEvent);

    // clean up extra tabs
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
  });
});

add_task(async function test_empty_states() {
  await PlacesUtils.history.clear();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    await navigateToCategoryAndWait(document, "history");

    let historyComponent = document.querySelector("view-history");
    historyComponent.profileAge = 8;
    await TestUtils.waitForCondition(() => historyComponent.emptyState);
    let emptyStateCard = historyComponent.emptyState;
    ok(
      emptyStateCard.headerEl.textContent.includes(
        "Get back to where you’ve been"
      ),
      "Initial empty state header has the expected text."
    );
    ok(
      emptyStateCard.descriptionEls[0].textContent.includes(
        "As you browse, the pages you visit will be listed here."
      ),
      "Initial empty state description has the expected text."
    );

    // Test empty state when History mode is set to never remember
    Services.prefs.setBoolPref(NEVER_REMEMBER_HISTORY_PREF, true);
    // Manually update the history component from the test, since changing this setting
    // in about:preferences will require a browser reload
    historyComponent.requestUpdate();
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);
    emptyStateCard = historyComponent.emptyState;
    ok(
      emptyStateCard.headerEl.textContent.includes("Nothing to show"),
      "Empty state with never remember history header has the expected text."
    );
    ok(
      emptyStateCard.descriptionEls[1].textContent.includes(
        "remember your activity as you browse. To change that"
      ),
      "Empty state with never remember history description has the expected text."
    );
    // Reset History mode to Remember
    Services.prefs.setBoolPref(NEVER_REMEMBER_HISTORY_PREF, false);
    // Manually update the history component from the test, since changing this setting
    // in about:preferences will require a browser reload
    historyComponent.requestUpdate();
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);

    // Test import history banner shows if profile age is 7 days or less and
    // user hasn't already imported history from another browser
    Services.prefs.setBoolPref(IMPORT_HISTORY_DISMISSED_PREF, false);
    Services.prefs.setBoolPref(HAS_IMPORTED_HISTORY_PREF, true);
    ok(!historyComponent.cards.length, "Import history banner not shown yet");
    historyComponent.profileAge = 0;
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);
    ok(
      !historyComponent.cards.length,
      "Import history banner still not shown yet"
    );
    Services.prefs.setBoolPref(HAS_IMPORTED_HISTORY_PREF, false);
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);
    ok(
      historyComponent.cards[0].textContent.includes(
        "Import history from another browser"
      ),
      "Import history banner is shown"
    );
    let importHistoryCloseButton =
      historyComponent.cards[0].querySelector("button.close");
    importHistoryCloseButton.click();
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);
    ok(
      Services.prefs.getBoolPref(IMPORT_HISTORY_DISMISSED_PREF, true) &&
        !historyComponent.cards.length,
      "Import history banner has been dismissed."
    );
    // Reset profileAge to greater than 7 to avoid affecting other tests
    historyComponent.profileAge = 8;
    Services.prefs.setBoolPref(IMPORT_HISTORY_DISMISSED_PREF, false);

    gBrowser.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test_observers_removed_when_view_is_hidden() {
  await PlacesUtils.history.clear();
  const NEW_TAB_URL = "https://example.com";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    NEW_TAB_URL
  );
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToCategoryAndWait(document, "history");
    const historyComponent = document.querySelector("view-history");
    historyComponent.profileAge = 8;
    let visitList = await TestUtils.waitForCondition(() =>
      historyComponent.cards?.[0]?.querySelector("fxview-tab-list")
    );
    info("The list should show a visit from the new tab.");
    await TestUtils.waitForCondition(() => visitList.rowEls.length === 1);

    let promiseHidden = BrowserTestUtils.waitForEvent(
      document,
      "visibilitychange"
    );
    await BrowserTestUtils.switchTab(gBrowser, tab);
    await promiseHidden;
    const { date } = await PlacesUtils.history
      .fetch(NEW_TAB_URL, {
        includeVisits: true,
      })
      .then(({ visits }) => visits[0]);
    await addHistoryItems(date);
    is(
      visitList.rowEls.length,
      1,
      "The list does not update when Firefox View is hidden."
    );

    info("The list should update when Firefox View is visible.");
    await openFirefoxViewTab(browser.ownerGlobal);
    visitList = await TestUtils.waitForCondition(() =>
      historyComponent.cards?.[0]?.querySelector("fxview-tab-list")
    );
    await TestUtils.waitForCondition(() => visitList.rowEls.length > 1);

    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function test_show_all_history_telemetry() {
  await PlacesUtils.history.clear();
  await addHistoryItems(today);
  await addHistoryItems(yesterday);
  await addHistoryItems(twoDaysAgo);
  await addHistoryItems(threeDaysAgo);
  await addHistoryItems(fourDaysAgo);
  await addHistoryItems(oneMonthAgo);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    await navigateToCategoryAndWait(document, "history");

    let historyComponent = document.querySelector("view-history");
    historyComponent.profileAge = 8;
    await historyComponentReady(historyComponent);

    await clearAllParentTelemetryEvents();
    let showAllHistoryBtn = historyComponent.showAllHistoryBtn;
    showAllHistoryBtn.scrollIntoView();
    await EventUtils.synthesizeMouseAtCenter(showAllHistoryBtn, {}, content);
    await showAllHistoryTelemetry();

    // Make sure library window is shown
    await TestUtils.waitForCondition(() =>
      Services.wm.getMostRecentWindow("Places:Organizer")
    );
    let library = Services.wm.getMostRecentWindow("Places:Organizer");
    await BrowserTestUtils.closeWindow(library);
    gBrowser.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test_search_history() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToCategoryAndWait(document, "history");
    const historyComponent = document.querySelector("view-history");
    historyComponent.profileAge = 8;
    await historyComponentReady(historyComponent);
    const searchTextbox = await TestUtils.waitForCondition(
      () => historyComponent.searchTextbox,
      "The search textbox is displayed."
    );

    info("Input a search query.");
    EventUtils.synthesizeMouseAtCenter(searchTextbox, {}, content);
    EventUtils.sendString("Example Domain 1", content);
    await BrowserTestUtils.waitForMutationCondition(
      historyComponent.shadowRoot,
      { childList: true, subtree: true },
      () =>
        historyComponent.cards.length === 1 &&
        document.l10n.getAttributes(
          historyComponent.cards[0].querySelector("[slot=header]")
        ).id === "firefoxview-search-results-header"
    );
    await TestUtils.waitForCondition(() => {
      const { rowEls } = historyComponent.lists[0];
      return rowEls.length === 1 && rowEls[0].mainEl.href === URLs[0];
    }, "There is one matching search result.");

    info("Input a bogus search query.");
    EventUtils.synthesizeMouseAtCenter(searchTextbox, {}, content);
    EventUtils.sendString("Bogus Query", content);
    await TestUtils.waitForCondition(() => {
      const tabList = historyComponent.lists[0];
      return tabList?.shadowRoot.querySelector("fxview-empty-state");
    }, "There are no matching search results.");

    info("Clear the search query.");
    EventUtils.synthesizeMouseAtCenter(searchTextbox.clearButton, {}, content);
    await BrowserTestUtils.waitForMutationCondition(
      historyComponent.shadowRoot,
      { childList: true, subtree: true },
      () =>
        historyComponent.cards.length ===
        historyComponent.historyMapByDate.length
    );
    searchTextbox.blur();

    info("Input a bogus search query with keyboard.");
    EventUtils.synthesizeKey("f", { accelKey: true }, content);
    EventUtils.sendString("Bogus Query", content);
    await TestUtils.waitForCondition(() => {
      const tabList = historyComponent.lists[0];
      return tabList?.shadowRoot.querySelector("fxview-empty-state");
    }, "There are no matching search results.");

    info("Clear the search query with keyboard.");
    is(
      historyComponent.shadowRoot.activeElement,
      searchTextbox,
      "Search input is focused"
    );
    EventUtils.synthesizeKey("KEY_Tab", {}, content);
    ok(
      searchTextbox.clearButton.matches(":focus-visible"),
      "Clear Search button is focused"
    );
    EventUtils.synthesizeKey("KEY_Enter", {}, content);
    await BrowserTestUtils.waitForMutationCondition(
      historyComponent.shadowRoot,
      { childList: true, subtree: true },
      () =>
        historyComponent.cards.length ===
        historyComponent.historyMapByDate.length
    );
  });
});

add_task(async function test_persist_collapse_card_after_view_change() {
  await PlacesUtils.history.clear();
  await addHistoryItems(today);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToCategoryAndWait(document, "history");
    const historyComponent = document.querySelector("view-history");
    historyComponent.profileAge = 8;
    await TestUtils.waitForCondition(
      () =>
        [...historyComponent.allHistoryItems.values()].reduce(
          (acc, { length }) => acc + length,
          0
        ) === 4
    );
    let firstHistoryCard = historyComponent.cards[0];
    ok(
      firstHistoryCard.isExpanded,
      "The first history card is expanded initially."
    );

    // Collapse history card
    EventUtils.synthesizeMouseAtCenter(firstHistoryCard.summaryEl, {}, content);
    is(
      firstHistoryCard.detailsEl.hasAttribute("open"),
      false,
      "The first history card is now collapsed."
    );

    // Switch to a new view and then back to History
    await navigateToCategoryAndWait(document, "syncedtabs");
    await navigateToCategoryAndWait(document, "history");

    // Check that first history card is still collapsed after changing view
    ok(
      !firstHistoryCard.isExpanded,
      "The first history card is still collapsed after changing view."
    );

    await PlacesUtils.history.clear();
    gBrowser.removeTab(gBrowser.selectedTab);
  });
});
