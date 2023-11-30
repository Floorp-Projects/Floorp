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
let tabSelectedTelemetry = [
  "firefoxview_next",
  "tab_selected",
  "toolbarbutton",
  undefined,
  {},
];
let enteredTelemetry = [
  "firefoxview_next",
  "entered",
  "firefoxview",
  undefined,
  { page: "recentbrowsing" },
];

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, true]] });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    clearHistory();
  });
});

add_task(async function firefox_view_entered_telemetry() {
  await clearAllParentTelemetryEvents();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
    let enteredAndTabSelectedEvents = [tabSelectedTelemetry, enteredTelemetry];
    await telemetryEvent(enteredAndTabSelectedEvents);

    enteredTelemetry[4] = { page: "recentlyclosed" };
    enteredAndTabSelectedEvents = [tabSelectedTelemetry, enteredTelemetry];

    navigateToCategory(document, "recentlyclosed");
    await clearAllParentTelemetryEvents();
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
    is(
      gBrowser.selectedBrowser.currentURI.spec,
      "about:robots",
      "The selected tab is about:robots"
    );
    await switchToFxViewTab(browser.ownerGlobal);
    await telemetryEvent(enteredAndTabSelectedEvents);
    await SpecialPowers.popPrefEnv();
    // clean up extra tabs
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
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
    await telemetryEvent(CARD_COLLAPSED_EVENT);
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
    await telemetryEvent(CARD_EXPANDED_EVENT);
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
    await telemetryEvent(changePageEvent);
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
    await telemetryEvent(changePageEvent);
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

    // Test history context menu options
    navigateToCategory(document, "history");
    let historyComponent = document.querySelector("view-history");
    await TestUtils.waitForCondition(() => historyComponent.fullyUpdated);
    await TestUtils.waitForCondition(
      () => historyComponent.lists[0].rowEls.length
    );
    let firstTabList = historyComponent.lists[0];
    let firstItem = firstTabList.rowEls[0];
    let panelList = historyComponent.panelList;
    await EventUtils.synthesizeMouseAtCenter(firstItem.buttonEl, {}, content);
    await BrowserTestUtils.waitForEvent(panelList, "shown");
    await clearAllParentTelemetryEvents();
    let panelItems = Array.from(panelList.children).filter(
      panelItem => panelItem.nodeName === "PANEL-ITEM"
    );
    let openInNewWindowOption = panelItems[1];
    let contextMenuEvent = [
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
    await telemetryEvent(contextMenuEvent);
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
    await telemetryEvent(contextMenuEvent);
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
    await telemetryEvent(contextMenuEvent);

    // clean up extra tabs
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
  });
});

add_task(async function test_browser_context_menu_telemetry() {
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, true]] });
  const menu = document.getElementById("contentAreaContextMenu");
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");
    await clearAllParentTelemetryEvents();

    // Test browser context menu options
    const openTabsComponent = document.querySelector("view-opentabs");
    await TestUtils.waitForCondition(
      () =>
        openTabsComponent.shadowRoot.querySelector("view-opentabs-card").tabList
          .rowEls.length
    );
    const [openTabsRow] =
      openTabsComponent.shadowRoot.querySelector("view-opentabs-card").tabList
        .rowEls;
    const promisePopup = BrowserTestUtils.waitForEvent(menu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(
      openTabsRow,
      { type: "contextmenu" },
      content
    );
    await promisePopup;
    const promiseNewWindow = BrowserTestUtils.waitForNewWindow();
    menu.activateItem(menu.querySelector("#context-openlink"));
    await telemetryEvent([
      [
        "firefoxview_next",
        "browser_context_menu",
        "tabs",
        null,
        { menu_action: "context-openlink", page: "recentbrowsing" },
      ],
    ]);

    // Clean up extra window
    const win = await promiseNewWindow;
    await BrowserTestUtils.closeWindow(win);
  });
  await SpecialPowers.popPrefEnv();
});
