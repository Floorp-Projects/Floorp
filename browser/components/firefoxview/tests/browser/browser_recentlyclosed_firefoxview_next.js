/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(globalThis, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});

const FXVIEW_NEXT_ENABLED_PREF = "browser.tabs.firefox-view-next";
const NEVER_REMEMBER_HISTORY_PREF = "browser.privatebrowsing.autostart";
const RECENTLY_CLOSED_EVENT = [
  ["firefoxview_next", "recently_closed", "tabs", undefined],
];
const DISMISS_CLOSED_TAB_EVENT = [
  ["firefoxview_next", "dismiss_closed_tab", "tabs", undefined],
];
const CHANGE_PAGE_EVENT = [
  ["firefoxview_next", "change_page", "navigation", undefined],
];
const CARD_COLLAPSED_EVENT = [
  ["firefoxview_next", "card_collapsed", "card_container", undefined],
];

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

async function dismiss_tab(itemElem, document) {
  // Make sure the firefoxview-next tab still has focus
  is(
    document.location.href,
    "about:firefoxview-next#recentlyclosed",
    "about:firefoxview-next is the selected tab and showing the Recently closed view page"
  );

  // Scroll to the tab element to ensure dismiss button is visible
  itemElem.scrollIntoView();
  is(isElInViewport(itemElem), true, "Tab is visible in viewport");

  info(`Dismissing tab ${itemElem.url}`);
  const closedObjectsChangePromise = TestUtils.topicObserved(
    "sessionstore-closed-objects-changed"
  );
  let dismissButton = itemElem.buttonEl;
  EventUtils.synthesizeMouseAtCenter(dismissButton, {}, document.ownerGlobal);
  await closedObjectsChangePromise;
}

async function prepareClosedTabs() {
  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  await open_then_close(URLs[0]);
  await open_then_close(URLs[1]);
  await open_then_close(URLs[2], newWin);
  await SimpleTest.promiseFocus(window);
  return async () => {
    info("Cleaning up after prepareClosedTabs");
    await promiseAllButPrimaryWindowClosed();
  };
}

async function recentlyClosedTelemetry() {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for recently_closed firefoxview_next telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    RECENTLY_CLOSED_EVENT,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

async function recentlyClosedDismissTelemetry() {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for dismiss_closed_tab firefoxview_next telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    DISMISS_CLOSED_TAB_EVENT,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

async function navigationTelemetry() {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for change_page firefoxview_next telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    CHANGE_PAGE_EVENT,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

async function cardCollapsedTelemetry() {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for card_collapsed firefoxview_next telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    CARD_COLLAPSED_EVENT,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, true]] });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    clearHistory();
  });
});

add_task(async function test_list_ordering() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Closed tab count after purging session history"
  );

  let cleanup = await prepareClosedTabs();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
    await clearAllParentTelemetryEvents();
    navigateToCategory(document, "recentlyclosed");
    await navigationTelemetry();

    let recentlyClosedComponent = document.querySelector(
      "view-recentlyclosed:not([slot=recentlyclosed])"
    );

    // Check that tabs list is rendered
    await TestUtils.waitForCondition(() => {
      return recentlyClosedComponent.cardEl;
    });
    let cardContainer = recentlyClosedComponent.cardEl;
    let cardMainSlotNode = Array.from(
      cardContainer?.mainSlot?.assignedNodes()
    )[0];
    is(
      cardMainSlotNode.tagName.toLowerCase(),
      "fxview-tab-list",
      "The tab list component is rendered."
    );

    let tabList = cardMainSlotNode.rowEls;

    is(tabList.length, 3, "Three tabs are shown in the list.");
    is(
      tabList[0].url,
      "https://example.net/",
      "First list item in recentlyclosed is in the correct order"
    );
    is(
      tabList[2].url,
      "http://mochi.test:8888/browser/",
      "Last list item in recentlyclosed is in the correct order"
    );

    let uri = tabList[0].url;
    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, uri);
    tabList[0].mainEl.click();
    await newTabPromise;

    gBrowser.removeTab(gBrowser.selectedTab);
  });
  await cleanup();
});

add_task(async function test_collapse_card() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Closed tab count after purging session history"
  );

  let cleanup = await prepareClosedTabs();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");

    let recentlyClosedComponent = document.querySelector(
      "view-recentlyclosed[slot=recentlyclosed]"
    );
    let cardContainer = recentlyClosedComponent.cardEl;
    is(
      cardContainer.isExpanded,
      true,
      "The card-container is expanded initially"
    );
    await clearAllParentTelemetryEvents();
    // Click the summary to collapse the details disclosure
    await EventUtils.synthesizeMouseAtCenter(
      cardContainer.summaryEl,
      {},
      content
    );
    is(
      cardContainer.detailsEl.hasAttribute("open"),
      false,
      "The card-container is collapsed"
    );
    await cardCollapsedTelemetry();
    // Click the summary again to expand the details disclosure
    await EventUtils.synthesizeMouseAtCenter(
      cardContainer.summaryEl,
      {},
      content
    );
    is(
      cardContainer.detailsEl.hasAttribute("open"),
      true,
      "The card-container is expanded"
    );

    // clean up extra tabs
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
  });
  await cleanup();
});

add_task(async function test_list_updates() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Closed tab count after purging session history"
  );

  let cleanup = await prepareClosedTabs();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
    navigateToCategory(document, "recentlyclosed");

    let recentlyClosedComponent = document.querySelector(
      "view-recentlyclosed:not([slot=recentlyclosed])"
    );

    // Check that tabs list is rendered
    await TestUtils.waitForCondition(() => {
      return recentlyClosedComponent.cardEl;
    });
    let cardContainer = recentlyClosedComponent.cardEl;
    let cardMainSlotNode = Array.from(
      cardContainer?.mainSlot?.assignedNodes()
    )[0];
    is(
      cardMainSlotNode.tagName.toLowerCase(),
      "fxview-tab-list",
      "The tab list component is rendered."
    );

    let tabList = cardMainSlotNode.rowEls;

    is(tabList.length, 3, "Three tabs are shown in the list.");
    let promiseClosedObjectsChanged = TestUtils.topicObserved(
      "sessionstore-closed-objects-changed"
    );
    await clearAllParentTelemetryEvents();
    await EventUtils.synthesizeMouseAtCenter(tabList[0].mainEl, {}, content);
    await promiseClosedObjectsChanged;
    await recentlyClosedTelemetry();

    await openFirefoxView(window);
    tabList = cardMainSlotNode.rowEls;
    is(tabList.length, 2, "Two tabs are shown in the list.");

    promiseClosedObjectsChanged = TestUtils.topicObserved(
      "sessionstore-closed-objects-changed"
    );
    await clearAllParentTelemetryEvents();
    await EventUtils.synthesizeMouseAtCenter(tabList[0].mainEl, {}, content);
    await promiseClosedObjectsChanged;
    await recentlyClosedTelemetry();
    await openFirefoxView(window);
    tabList = cardMainSlotNode.rowEls;
    is(tabList.length, 1, "One tab is shown in the list.");

    // clean up extra tabs
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
  });
  await cleanup();
});

/**
 * Asserts that tabs that have been recently closed can be
 * dismissed by clicking on their respective dismiss buttons.
 */
add_task(async function test_dismiss_tab() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCountForWindow(window),
    0,
    "Closed tab count after purging session history"
  );

  let cleanup = await prepareClosedTabs();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    navigateToCategory(document, "recentlyclosed");

    let recentlyClosedComponent = document.querySelector(
      "view-recentlyclosed:not([slot=recentlyclosed])"
    );

    // Check that tabs list is rendered
    await TestUtils.waitForCondition(() => {
      return recentlyClosedComponent.cardEl;
    });
    let cardContainer = recentlyClosedComponent.cardEl;
    let cardMainSlotNode = Array.from(
      cardContainer?.mainSlot?.assignedNodes()
    )[0];
    let tabList = cardMainSlotNode.rowEls;

    info("calling dismiss_tab");
    await clearAllParentTelemetryEvents();
    await dismiss_tab(tabList[0], document);
    info("waiting for list re-render");
    await recentlyClosedComponent.getUpdateComplete();
    info("check results");
    await recentlyClosedDismissTelemetry();
    Assert.equal(SessionStore.getClosedTabCountForWindow(window), 2);
    tabList = cardMainSlotNode.rowEls;

    Assert.equal(
      tabList[0].url,
      URLs[1],
      `First recently closed item should be ${URLs[1]}`
    );

    Assert.equal(
      tabList.length,
      2,
      "recentlyclosed should have two list items"
    );

    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
  });
  await cleanup();
});

add_task(async function test_empty_states() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCountForWindow(window),
    0,
    "Closed tab count after purging session history"
  );
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");

    navigateToCategory(document, "recentlyclosed");
    let recentlyClosedComponent = document.querySelector(
      "view-recentlyclosed:not([slot=recentlyclosed])"
    );

    await TestUtils.waitForCondition(() => recentlyClosedComponent.emptyState);
    let emptyStateCard = recentlyClosedComponent.emptyState;
    ok(
      emptyStateCard.headerEl.textContent.includes("Closed a tab too soon"),
      "Initial empty state header has the expected text."
    );
    ok(
      emptyStateCard.descriptionEls[0].textContent.includes(
        "Here you’ll find the tabs you recently closed"
      ),
      "Initial empty state description has the expected text."
    );

    // Test empty state when History mode is set to never remember
    Services.prefs.setBoolPref(NEVER_REMEMBER_HISTORY_PREF, true);
    // Manually update the recentlyclosed component from the test, since changing this setting
    // in about:preferences will require a browser reload
    recentlyClosedComponent.requestUpdate();
    await TestUtils.waitForCondition(
      () => recentlyClosedComponent.fullyUpdated
    );
    emptyStateCard = recentlyClosedComponent.emptyState;
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
    gBrowser.removeTab(gBrowser.selectedTab);
  });
});
