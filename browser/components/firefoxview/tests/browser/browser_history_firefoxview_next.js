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

const FXVIEW_NEXT_ENABLED_PREF = "browser.tabs.firefox-view-next";
const NEVER_REMEMBER_HISTORY_PREF = "browser.privatebrowsing.autostart";
const DAY_MS = 24 * 60 * 60 * 1000;
const today = new Date();
const yesterday = new Date(Date.now() - DAY_MS);
const twoDaysAgo = new Date(Date.now() - DAY_MS * 2);
const threeDaysAgo = new Date(Date.now() - DAY_MS * 3);
const fourDaysAgo = new Date(Date.now() - DAY_MS * 4);
const oneMonthAgo = new Date(today);
oneMonthAgo.setMonth(
  oneMonthAgo.getMonth() === 0 ? 11 : oneMonthAgo.getMonth() - 1
);

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

async function openFirefoxView(win) {
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#firefox-view-button",
    { type: "mousedown" },
    win.browsingContext
  );
}

function navigateToHistory(document) {
  // Navigate to Recently closed tabs page/view
  const navigation = document.querySelector("fxview-category-navigation");
  let historyNavButton = Array.from(navigation.categoryButtons).find(
    categoryButton => {
      return categoryButton.name === "history";
    }
  );
  historyNavButton.buttonEl.click();
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
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, true]] });
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
    is(document.location.href, "about:firefoxview-next");

    navigateToHistory(document);

    let historyComponent = document.querySelector("view-history");
    historyComponent.profileAge = 8;
    await TestUtils.waitForCondition(
      () =>
        [...historyComponent.allHistoryItems.values()].reduce(
          (acc, { length }) => acc + length,
          0
        ) === 24
    );

    let expectedNumOfCards = historyComponent.historyMapByDate.length;

    let cards = historyComponent.cards;
    let actualNumOfCards = cards.length;

    is(
      expectedNumOfCards,
      actualNumOfCards,
      `Total number of cards should be ${expectedNumOfCards}`
    );

    let firstCard = cards[0];

    info("The first card should have a header for 'Today'.");
    await BrowserTestUtils.waitForMutationCondition(
      firstCard.querySelector("[slot=header]"),
      { attributes: true },
      () =>
        document.l10n.getAttributes(firstCard.querySelector("[slot=header]"))
          .id === "firefoxview-history-date-today"
    );

    // Test number of cards when sorted by site/domain
    historyComponent.sortOption = "site";
    await historyComponent.updateHistoryData();
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);

    expectedNumOfCards = historyComponent.historyMapBySite.length;

    info(`Total number of cards should be ${expectedNumOfCards}`);
    await BrowserTestUtils.waitForMutationCondition(
      historyComponent.shadowRoot,
      { childList: true, subtree: true },
      () => expectedNumOfCards === historyComponent.cards.length
    );
    gBrowser.removeTab(gBrowser.selectedTab);
  });

  add_task(async function test_empty_states() {
    await PlacesUtils.history.clear();
    await withFirefoxView({}, async browser => {
      const { document } = browser.contentWindow;
      is(document.location.href, "about:firefoxview-next");

      navigateToHistory(document);

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
});
