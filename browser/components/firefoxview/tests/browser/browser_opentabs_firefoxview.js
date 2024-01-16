/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL1 = "about:robots";
const TEST_URL2 = "https://example.org/";
const TEST_URL3 = "about:mozilla";

const fxaDevicesWithCommands = [
  {
    id: 1,
    name: "My desktop device",
    availableCommands: { "https://identity.mozilla.com/cmd/open-uri": "test" },
    lastAccessTime: Date.now(),
  },
  {
    id: 2,
    name: "My mobile device",
    availableCommands: { "https://identity.mozilla.com/cmd/open-uri": "boo" },
    lastAccessTime: Date.now() + 60000, // add 30min
  },
];

function getCards(openTabs) {
  return openTabs.shadowRoot.querySelectorAll("view-opentabs-card");
}

async function getRowsForCard(card) {
  await TestUtils.waitForCondition(() => card.tabList.rowEls.length);
  return card.tabList.rowEls;
}

async function getContextMenuPanelListForCard(card) {
  let menuContainer = card.shadowRoot.querySelector(
    "view-opentabs-contextmenu"
  );
  ok(menuContainer, "Found the menuContainer for card");
  await TestUtils.waitForCondition(
    () => menuContainer.panelList,
    "Waiting for the context menu's panel-list to be rendered"
  );
  ok(
    menuContainer.panelList,
    "Found the panelList in the card's view-opentabs-contextmenu"
  );
  return menuContainer.panelList;
}

async function openContextMenuForItem(tabItem, card) {
  // click on the item's button element (more menu)
  // and wait for the panel list to be shown
  tabItem.buttonEl.click();
  // NOTE: menu must populate with devices data before it can be rendered
  // so the creation of the panel-list can be async
  let panelList = await getContextMenuPanelListForCard(card);
  await BrowserTestUtils.waitForEvent(panelList, "shown");
  return panelList;
}

async function moreMenuSetup(document) {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL2);
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL3);

  // once we've opened a few tabs, navigate to the open tabs section in firefox view
  await clickFirefoxViewButton(window);
  navigateToCategory(document, "opentabs");

  let openTabs = document.querySelector("view-opentabs[name=opentabs]");

  let cards;
  await TestUtils.waitForCondition(() => {
    cards = getCards(openTabs);
    return cards.length == 1;
  });
  is(cards.length, 1, "There is one open window.");

  let rows = await getRowsForCard(cards[0]);
  is(rows.length, 3, "There are three tabs in the open tabs list.");

  let firstTab = rows[0];

  firstTab.scrollIntoView();
  is(
    isElInViewport(firstTab),
    true,
    "first tab list item is visible in viewport"
  );

  return [cards, rows];
}

add_task(async function test_more_menus() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;
    let shown, menuHidden;

    gBrowser.selectedTab = gBrowser.visibleTabs[0];
    ok(
      gBrowser.selectedTab.linkedBrowser.currentURI.spec == "about:blank",
      "Selected tab is about:blank"
    );

    win.gURLBar.focus();
    win.gURLBar.value = TEST_URL1;
    EventUtils.synthesizeKey("KEY_Enter", {}, win);

    let [cards, rows] = await moreMenuSetup(document);
    let firstTab = rows[0];
    // Open the panel list (more menu) from the first list item
    let panelList = await openContextMenuForItem(firstTab, cards[0]);

    // Close Tab menu item
    info("Panel list shown. Clicking on panel-item");
    let panelItem = panelList.querySelector(
      "panel-item[data-l10n-id=fxviewtabrow-close-tab]"
    );
    let panelItemButton = panelItem.shadowRoot.querySelector(
      "button[role=menuitem]"
    );
    ok(panelItem, "Close Tab panel item exists");
    ok(
      panelItemButton,
      "Close Tab panel item button with role=menuitem exists"
    );

    await clearAllParentTelemetryEvents();
    let contextMenuEvent = [
      [
        "firefoxview_next",
        "context_menu",
        "tabs",
        undefined,
        { menu_action: "close-tab", data_type: "opentabs" },
      ],
    ];

    // close a tab via the menu
    menuHidden = BrowserTestUtils.waitForEvent(panelList, "hidden");
    panelItemButton.click();
    await cards[0].getUpdateComplete();
    await menuHidden;
    await telemetryEvent(contextMenuEvent);

    let visibleTabs = gBrowser.visibleTabs;
    is(visibleTabs.length, 2, "Expected to now have 2 open tabs");

    // Move Tab submenu item
    firstTab = rows[0];
    is(firstTab.url, TEST_URL2, `First tab list item is ${TEST_URL2}`);

    is(
      visibleTabs[0].linkedBrowser.currentURI.spec,
      TEST_URL2,
      `First tab in tab strip is ${TEST_URL2}`
    );
    is(
      visibleTabs[visibleTabs.length - 1].linkedBrowser.currentURI.spec,
      TEST_URL3,
      `Last tab in tab strip is ${TEST_URL3}`
    );

    panelList = await openContextMenuForItem(firstTab, cards[0]);
    let moveTabsPanelItem = panelList.querySelector(
      "panel-item[data-l10n-id=fxviewtabrow-move-tab]"
    );

    let moveTabsSubmenuList = moveTabsPanelItem.shadowRoot.querySelector(
      "panel-list[id=move-tab-menu]"
    );
    ok(moveTabsSubmenuList, "Move tabs submenu panel list exists");

    // navigate down to the "Move tabs" submenu option, and
    // open it with the right arrow key
    EventUtils.synthesizeKey("KEY_ArrowDown", {});
    shown = BrowserTestUtils.waitForEvent(moveTabsSubmenuList, "shown");
    EventUtils.synthesizeKey("KEY_ArrowRight", {});
    await shown;

    await clearAllParentTelemetryEvents();
    contextMenuEvent = [
      [
        "firefoxview_next",
        "context_menu",
        "tabs",
        null,
        { menu_action: "move-tab-end", data_type: "opentabs" },
      ],
    ];

    // click on the first option, which should be "Move to the end" since
    // this is the first tab
    menuHidden = BrowserTestUtils.waitForEvent(panelList, "hidden");
    EventUtils.synthesizeKey("KEY_Enter", {});
    await telemetryEvent(contextMenuEvent);
    await menuHidden;

    visibleTabs = gBrowser.visibleTabs;
    is(
      visibleTabs[0].linkedBrowser.currentURI.spec,
      TEST_URL3,
      `First tab in tab strip is now ${TEST_URL3}`
    );
    is(
      visibleTabs[visibleTabs.length - 1].linkedBrowser.currentURI.spec,
      TEST_URL2,
      `Last tab in tab strip is now ${TEST_URL2}`
    );

    // this entire "move tabs" submenu test can be reordered above
    // closing a tab since it very clearly reveals the issues
    // outlined in bug 1852622 when there are 3 or more tabs open
    // and one is moved via the more menus.
    await BrowserTestUtils.waitForMutationCondition(
      cards[0].shadowRoot,
      { characterData: true, childList: true, subtree: true },
      async () => {
        rows = await getRowsForCard(cards[0]);
        firstTab = rows[0];
        return firstTab.url == TEST_URL3;
      }
    );

    // Copy Link menu item (copyLink function that's called is a member of Viewpage.mjs)
    panelList = await openContextMenuForItem(firstTab, cards[0]);
    firstTab = rows[0];
    panelItem = panelList.querySelector(
      "panel-item[data-l10n-id=fxviewtabrow-copy-link]"
    );
    panelItemButton = panelItem.shadowRoot.querySelector(
      "button[role=menuitem]"
    );
    ok(panelItem, "Copy link panel item exists");
    ok(
      panelItemButton,
      "Copy link panel item button with role=menuitem exists"
    );

    await clearAllParentTelemetryEvents();
    contextMenuEvent = [
      [
        "firefoxview_next",
        "context_menu",
        "tabs",
        null,
        { menu_action: "copy-link", data_type: "opentabs" },
      ],
    ];

    menuHidden = BrowserTestUtils.waitForEvent(panelList, "hidden");
    panelItemButton.click();
    info("Waiting for menuHidden");
    await menuHidden;
    info("Waiting for telemetryEvent");
    await telemetryEvent(contextMenuEvent);

    let copiedText = SpecialPowers.getClipboardData(
      "text/plain",
      Ci.nsIClipboard.kGlobalClipboard
    );
    is(copiedText, TEST_URL3, "The correct url has been copied and pasted");

    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs[0]);
    }
  });
});

add_task(async function test_send_device_submenu() {
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
        tabs: [],
      },
    ],
  });
  sandbox
    .stub(gSync, "getSendTabTargets")
    .callsFake(() => fxaDevicesWithCommands);

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let shown;

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    let [cards, rows] = await moreMenuSetup(document);

    let firstTab = rows[0];
    let panelList = await openContextMenuForItem(firstTab, cards[0]);

    let sendTabPanelItem = panelList.querySelector(
      "panel-item[data-l10n-id=fxviewtabrow-send-tab]"
    );

    ok(sendTabPanelItem, "Send tabs to device submenu panel item exists");

    let sendTabSubmenuList = sendTabPanelItem.shadowRoot.querySelector(
      "panel-list[id=send-tab-menu]"
    );
    ok(sendTabSubmenuList, "Send tabs to device submenu panel list exists");

    // navigate down to the "Send tabs" submenu option, and
    // open it with the right arrow key
    EventUtils.synthesizeKey("KEY_ArrowDown", {});
    EventUtils.synthesizeKey("KEY_ArrowDown", {});
    EventUtils.synthesizeKey("KEY_ArrowDown", {});

    shown = BrowserTestUtils.waitForEvent(sendTabSubmenuList, "shown");
    EventUtils.synthesizeKey("KEY_ArrowRight", {});
    await shown;

    let expectation = sandbox
      .mock(gSync)
      .expects("sendTabToDevice")
      .once()
      .withExactArgs(
        TEST_URL2,
        [fxaDevicesWithCommands[0]],
        "mochitest index /"
      )
      .returns(true);

    await clearAllParentTelemetryEvents();
    let contextMenuEvent = [
      [
        "firefoxview_next",
        "context_menu",
        "tabs",
        null,
        { menu_action: "send-tab-device", data_type: "opentabs" },
      ],
    ];

    // click on the first device and verify it was "sent"
    let menuHidden = BrowserTestUtils.waitForEvent(panelList, "hidden");
    EventUtils.synthesizeKey("KEY_Enter", {});

    expectation.verify();
    await telemetryEvent(contextMenuEvent);
    await menuHidden;

    sandbox.restore();
    TabsSetupFlowManager.resetInternalState();
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs[0]);
    }
  });
});
