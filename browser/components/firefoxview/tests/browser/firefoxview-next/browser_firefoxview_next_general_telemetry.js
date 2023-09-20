/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../head.js */

const FXVIEW_NEXT_ENABLED_PREF = "browser.tabs.firefox-view-next";
const CARD_COLLAPSED_EVENT = [
  ["firefoxview_next", "card_collapsed", "card_container", undefined],
];
const CARD_EXPANDED_EVENT = [
  ["firefoxview_next", "card_expanded", "card_container", undefined],
];

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

async function cardExpandedTelemetry() {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for card_expanded firefoxview_next telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    CARD_EXPANDED_EVENT,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

async function navigationTelemetry(changePageEvent) {
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
    changePageEvent,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

async function contextMenuTelemetry(contextMenuEvent) {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for context_menu firefoxview_next telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    contextMenuEvent,
    { category: "firefoxview_next" },
    { clear: true, process: "parent" }
  );
}

async function enteredTelemetry(enteredEvent) {
  await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      ).parent;
      return events && events.length >= 1;
    },
    "Waiting for entered firefoxview_next telemetry event.",
    200,
    100
  );

  TelemetryTestUtils.assertEvents(
    enteredEvent,
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

add_task(async function test_collapse_and_expand_card() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");

    // Test using Recently Closed card on Recent Browsing page
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
    await cardExpandedTelemetry();
  });
});

add_task(async function test_change_page_telemetry() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
    let changePageEvent = [
      [
        "firefoxview_next",
        "change_page",
        "navigation",
        undefined,
        { page: "recentlyclosed", source: "category-navigation" },
      ],
    ];
    await clearAllParentTelemetryEvents();
    navigateToCategory(document, "recentlyclosed");
    await navigationTelemetry(changePageEvent);
    navigateToCategory(document, "recentbrowsing");

    let openTabsComponent = document.querySelector(
      "view-opentabs[slot=opentabs]"
    );
    let cardContainer =
      openTabsComponent.shadowRoot.querySelector("view-opentabs-card").cardEl;
    let viewAllLink = cardContainer.viewAllLink;
    changePageEvent = [
      [
        "firefoxview_next",
        "change_page",
        "navigation",
        undefined,
        { page: "opentabs", source: "view-all" },
      ],
    ];
    await clearAllParentTelemetryEvents();
    await EventUtils.synthesizeMouseAtCenter(viewAllLink, {}, content);
    await navigationTelemetry(changePageEvent);
  });
});

add_task(async function test_context_menu_telemetry() {
  await PlacesUtils.history.insert({
    url: URLs[0],
    title: "Example Domain 1",
    visits: [{ date: new Date() }],
  });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");

    // Test open tabs telemetry
    let openTabsComponent = document.querySelector(
      "view-opentabs[slot=opentabs]"
    );
    let tabList =
      openTabsComponent.shadowRoot.querySelector("view-opentabs-card").tabList;
    let firstItem = tabList.rowEls[0];
    let panelList =
      openTabsComponent.shadowRoot.querySelector(
        "view-opentabs-card"
      ).panelList;
    await EventUtils.synthesizeMouseAtCenter(firstItem.buttonEl, {}, content);
    await BrowserTestUtils.waitForEvent(panelList, "shown");
    await clearAllParentTelemetryEvents();
    let copyLinkOption = panelList.children[1];
    let contextMenuEvent = [
      [
        "firefoxview_next",
        "context_menu",
        "tabs",
        undefined,
        { menu_action: "copy-link", data_type: "opentabs" },
      ],
    ];
    await EventUtils.synthesizeMouseAtCenter(copyLinkOption, {}, content);
    await contextMenuTelemetry(contextMenuEvent);

    // Open new tab to test 'Close tab' menu option
    window.openTrustedLinkIn("about:robots", "tab");
    await switchToFxViewTab(browser.ownerGlobal);
    firstItem = tabList.rowEls[0];
    await EventUtils.synthesizeMouseAtCenter(firstItem.buttonEl, {}, content);
    await BrowserTestUtils.waitForEvent(panelList, "shown");
    await clearAllParentTelemetryEvents();
    let closeTabOption = panelList.children[0];
    contextMenuEvent = [
      [
        "firefoxview_next",
        "context_menu",
        "tabs",
        undefined,
        { menu_action: "close-tab", data_type: "opentabs" },
      ],
    ];
    await EventUtils.synthesizeMouseAtCenter(closeTabOption, {}, content);
    await contextMenuTelemetry(contextMenuEvent);

    // Test history context menu options
    navigateToCategory(document, "history");
    let historyComponent = document.querySelector("view-history");
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);
    let firstTabList = historyComponent.lists[0];
    firstItem = firstTabList.rowEls[0];
    panelList = historyComponent.panelList;
    await EventUtils.synthesizeMouseAtCenter(firstItem.buttonEl, {}, content);
    await BrowserTestUtils.waitForEvent(panelList, "shown");
    await clearAllParentTelemetryEvents();
    let panelItems = Array.from(panelList.children).filter(
      panelItem => panelItem.nodeName === "PANEL-ITEM"
    );
    let openInNewWindowOption = panelItems[1];
    contextMenuEvent = [
      [
        "firefoxview_next",
        "context_menu",
        "tabs",
        undefined,
        { menu_action: "open-in-new-window", data_type: "history" },
      ],
    ];
    let newWindowPromise = BrowserTestUtils.waitForNewWindow({
      url: URLs[0],
    });
    await EventUtils.synthesizeMouseAtCenter(
      openInNewWindowOption,
      {},
      content
    );
    let win = await newWindowPromise;
    await contextMenuTelemetry(contextMenuEvent);
    await BrowserTestUtils.closeWindow(win);

    await EventUtils.synthesizeMouseAtCenter(firstItem.buttonEl, {}, content);
    await BrowserTestUtils.waitForEvent(panelList, "shown");
    await clearAllParentTelemetryEvents();
    let openInPrivateWindowOption = panelItems[2];
    contextMenuEvent = [
      [
        "firefoxview_next",
        "context_menu",
        "tabs",
        undefined,
        { menu_action: "open-in-private-window", data_type: "history" },
      ],
    ];
    newWindowPromise = BrowserTestUtils.waitForNewWindow({
      url: URLs[0],
    });
    await EventUtils.synthesizeMouseAtCenter(
      openInPrivateWindowOption,
      {},
      content
    );
    win = await newWindowPromise;
    await contextMenuTelemetry(contextMenuEvent);
    ok(
      PrivateBrowsingUtils.isWindowPrivate(win),
      "Should have opened a private window."
    );
    await BrowserTestUtils.closeWindow(win);

    await EventUtils.synthesizeMouseAtCenter(firstItem.buttonEl, {}, content);
    await BrowserTestUtils.waitForEvent(panelList, "shown");
    await clearAllParentTelemetryEvents();
    let deleteFromHistoryOption = panelItems[0];
    contextMenuEvent = [
      [
        "firefoxview_next",
        "context_menu",
        "tabs",
        undefined,
        { menu_action: "delete-from-history", data_type: "history" },
      ],
    ];
    await EventUtils.synthesizeMouseAtCenter(
      deleteFromHistoryOption,
      {},
      content
    );
    await contextMenuTelemetry(contextMenuEvent);

    // clean up extra tabs
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
  });
});

add_task(async function firefox_view_entered_telemetry() {
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, true]] });
  await clearAllParentTelemetryEvents();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
    let enteredEvent = [
      [
        "firefoxview_next",
        "entered",
        "firefoxview",
        null,
        { page: "recentbrowsing" },
      ],
    ];
    await enteredTelemetry(enteredEvent);

    enteredEvent = [
      [
        "firefoxview_next",
        "entered",
        "firefoxview",
        null,
        { page: "recentlyclosed" },
      ],
    ];

    navigateToCategory(document, "recentlyclosed");
    await clearAllParentTelemetryEvents();
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
    is(
      gBrowser.selectedBrowser.currentURI.spec,
      "about:robots",
      "The selected tab is about:robots"
    );
    await switchToFxViewTab(browser.ownerGlobal);
    await enteredTelemetry(enteredEvent);
    await SpecialPowers.popPrefEnv();
    // clean up extra tabs
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
  });
});
