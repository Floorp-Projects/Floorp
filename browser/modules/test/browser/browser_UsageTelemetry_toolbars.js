/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

gReduceMotionOverride = true;

function enterCustomizationMode(win = window) {
  let customizationReadyPromise = BrowserTestUtils.waitForEvent(
    win.gNavToolbox,
    "customizationready"
  );
  win.gCustomizeMode.enter();
  return customizationReadyPromise;
}

function leaveCustomizationMode(win = window) {
  let customizationDonePromise = BrowserTestUtils.waitForEvent(
    win.gNavToolbox,
    "aftercustomization"
  );
  win.gCustomizeMode.exit();
  return customizationDonePromise;
}

Services.prefs.setBoolPref("browser.uiCustomization.skipSourceNodeCheck", true);
registerCleanupFunction(() => {
  CustomizableUI.reset();
  Services.prefs.clearUserPref("browser.uiCustomization.skipSourceNodeCheck");
});

// Stolen from browser/components/customizableui/tests/browser/head.js
function simulateItemDrag(aToDrag, aTarget, aEvent = {}, aOffset = 2) {
  let ev = aEvent;
  if (ev == "end" || ev == "start") {
    let win = aTarget.ownerGlobal;
    const dwu = win.windowUtils;
    let bounds = dwu.getBoundsWithoutFlushing(aTarget);
    if (ev == "end") {
      ev = {
        clientX: bounds.right - aOffset,
        clientY: bounds.bottom - aOffset,
      };
    } else {
      ev = { clientX: bounds.left + aOffset, clientY: bounds.top + aOffset };
    }
  }
  ev._domDispatchOnly = true;
  EventUtils.synthesizeDrop(
    aToDrag.parentNode,
    aTarget,
    null,
    null,
    aToDrag.ownerGlobal,
    aTarget.ownerGlobal,
    ev
  );
  // Ensure dnd suppression is cleared.
  EventUtils.synthesizeMouseAtCenter(
    aTarget,
    { type: "mouseup" },
    aTarget.ownerGlobal
  );
}

function organizeToolbars(state = {}) {
  // Set up the defaults for the state.
  let targetState = Object.assign(
    {
      // Areas where widgets can be placed, set to an array of widget IDs.
      "toolbar-menubar": undefined,
      PersonalToolbar: undefined,
      TabsToolbar: ["tabbrowser-tabs", "alltabs-button"],
      "widget-overflow-fixed-list": undefined,
      "nav-bar": ["back-button", "forward-button", "urlbar-container"],

      // The page action's that should be in the URL bar.
      pageActionsInUrlBar: [],

      // Areas to show or hide.
      titlebarVisible: false,
      menubarVisible: false,
      personalToolbarVisible: false,
    },
    state
  );

  for (let area of CustomizableUI.areas) {
    // Clear out anything there already.
    for (let widgetId of CustomizableUI.getWidgetIdsInArea(area)) {
      CustomizableUI.removeWidgetFromArea(widgetId);
    }

    if (targetState[area]) {
      // We specify the position explicitly to support the toolbars that have
      // fixed widgets.
      let position = 0;
      for (let widgetId of targetState[area]) {
        CustomizableUI.addWidgetToArea(widgetId, area, position++);
      }
    }
  }

  CustomizableUI.setToolbarVisibility(
    "toolbar-menubar",
    targetState.menubarVisible
  );
  CustomizableUI.setToolbarVisibility(
    "PersonalToolbar",
    targetState.personalToolbarVisible
  );

  Services.prefs.setBoolPref(
    "browser.tabs.drawInTitlebar",
    !targetState.titlebarVisible
  );

  for (let action of PageActions.actions) {
    action.pinnedToUrlbar = targetState.pageActionsInUrlBar.includes(action.id);
  }

  // Clear out the existing telemetry.
  Services.telemetry.getSnapshotForKeyedScalars("main", true);
}

function assertVisibilityScalars(expected) {
  let scalars =
    Services.telemetry.getSnapshotForKeyedScalars("main", true)?.parent?.[
      "browser.ui.toolbar_widgets"
    ] ?? {};

  // Only some platforms have the menubar items.
  if (AppConstants.MENUBAR_CAN_AUTOHIDE) {
    expected.push("menubar-items_pinned_menu-bar");
  }

  let keys = new Set(expected.concat(Object.keys(scalars)));
  for (let key of keys) {
    Assert.ok(expected.includes(key), `Scalar key ${key} was unexpected.`);
    Assert.ok(scalars[key], `Expected to see see scalar key ${key} be true.`);
  }
}

function assertCustomizeScalars(expected) {
  let scalars =
    Services.telemetry.getSnapshotForKeyedScalars("main", true)?.parent?.[
      "browser.ui.customized_widgets"
    ] ?? {};

  let keys = new Set(Object.keys(expected).concat(Object.keys(scalars)));
  for (let key of keys) {
    Assert.equal(
      scalars[key],
      expected[key],
      `Expected to see the correct value for scalar ${key}.`
    );
  }
}

add_task(async function widgetPositions() {
  organizeToolbars();

  BrowserUsageTelemetry._recordUITelemetry();

  assertVisibilityScalars([
    "menu-toolbar_pinned_off",
    "titlebar_pinned_off",
    "bookmarks-bar_pinned_off",

    "tabbrowser-tabs_pinned_tabs-bar",
    "alltabs-button_pinned_tabs-bar",

    "forward-button_pinned_nav-bar-start",
    "back-button_pinned_nav-bar-start",
  ]);

  organizeToolbars({
    PersonalToolbar: [
      "fxa-toolbar-menu-button",
      "new-tab-button",
      "developer-button",
    ],

    TabsToolbar: [
      "stop-reload-button",
      "tabbrowser-tabs",
      "personal-bookmarks",
    ],

    "nav-bar": [
      "home-button",
      "forward-button",
      "downloads-button",
      "urlbar-container",
      "back-button",
      "library-button",
    ],

    personalToolbarVisible: true,
  });

  BrowserUsageTelemetry._recordUITelemetry();

  assertVisibilityScalars([
    "menu-toolbar_pinned_off",
    "titlebar_pinned_off",
    "bookmarks-bar_pinned_on",

    "tabbrowser-tabs_pinned_tabs-bar",
    "stop-reload-button_pinned_tabs-bar",
    "personal-bookmarks_pinned_tabs-bar",
    "alltabs-button_pinned_tabs-bar",

    "home-button_pinned_nav-bar-start",
    "forward-button_pinned_nav-bar-start",
    "downloads-button_pinned_nav-bar-start",
    "back-button_pinned_nav-bar-end",
    "library-button_pinned_nav-bar-end",

    "fxa-toolbar-menu-button_pinned_bookmarks-bar",
    "new-tab-button_pinned_bookmarks-bar",
    "developer-button_pinned_bookmarks-bar",
  ]);

  CustomizableUI.reset();
});

add_task(async function customizeMode() {
  for (let bookmarksFeatureEnabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.toolbars.bookmarks.2h2020", bookmarksFeatureEnabled]],
    });

    // Create a default state.
    organizeToolbars({
      PersonalToolbar: ["personal-bookmarks"],

      TabsToolbar: ["tabbrowser-tabs", "new-tab-button"],

      "nav-bar": [
        "back-button",
        "forward-button",
        "stop-reload-button",
        "urlbar-container",
        "home-button",
        "library-button",
      ],
    });

    BrowserUsageTelemetry._recordUITelemetry();

    assertVisibilityScalars([
      "menu-toolbar_pinned_off",
      "titlebar_pinned_off",
      "bookmarks-bar_pinned_off",

      "tabbrowser-tabs_pinned_tabs-bar",
      "new-tab-button_pinned_tabs-bar",
      "alltabs-button_pinned_tabs-bar",

      "back-button_pinned_nav-bar-start",
      "forward-button_pinned_nav-bar-start",
      "stop-reload-button_pinned_nav-bar-start",
      "home-button_pinned_nav-bar-end",
      "library-button_pinned_nav-bar-end",

      "personal-bookmarks_pinned_bookmarks-bar",
    ]);

    let win = await BrowserTestUtils.openNewBrowserWindow();

    await enterCustomizationMode(win);

    let toolbarButton = win.document.getElementById(
      "customization-toolbar-visibility-button"
    );
    let toolbarPopup = win.document.getElementById(
      "customization-toolbar-menu"
    );
    let popupShown = BrowserTestUtils.waitForEvent(toolbarPopup, "popupshown");
    EventUtils.synthesizeMouseAtCenter(toolbarButton, {}, win);
    await popupShown;

    let barMenu = win.document.getElementById("toggle_PersonalToolbar");
    let popupHidden = BrowserTestUtils.waitForEvent(
      toolbarPopup,
      "popuphidden"
    );
    if (bookmarksFeatureEnabled) {
      let subMenu = barMenu.querySelector("menupopup");
      popupShown = BrowserTestUtils.waitForEvent(subMenu, "popupshown");
      EventUtils.synthesizeMouseAtCenter(barMenu, {}, win);
      await popupShown;
      let alwaysButton = barMenu.querySelector(
        '*[data-visibility-enum="always"]'
      );
      EventUtils.synthesizeMouseAtCenter(alwaysButton, {}, win);
    } else {
      EventUtils.synthesizeMouseAtCenter(barMenu, {}, win);
    }
    await popupHidden;

    let navbar = CustomizableUI.getCustomizationTarget(
      win.document.getElementById("nav-bar")
    );
    let bookmarksBar = CustomizableUI.getCustomizationTarget(
      win.document.getElementById("PersonalToolbar")
    );
    let tabBar = CustomizableUI.getCustomizationTarget(
      win.document.getElementById("TabsToolbar")
    );

    simulateItemDrag(
      win.document.getElementById("home-button"),
      navbar,
      "start"
    );
    simulateItemDrag(
      win.document.getElementById("library-button"),
      bookmarksBar
    );
    simulateItemDrag(win.document.getElementById("stop-reload-button"), tabBar);
    simulateItemDrag(
      win.document.getElementById("stop-reload-button"),
      navbar,
      "start"
    );
    simulateItemDrag(win.document.getElementById("stop-reload-button"), tabBar);

    await leaveCustomizationMode(win);

    await BrowserTestUtils.closeWindow(win);

    let bookmarksBarTelemetryScalar = bookmarksFeatureEnabled
      ? "bookmarks-bar_move_off_always_customization-toolbar-menu"
      : "bookmarks-bar_move_off_on_customization-toolbar-menu";
    assertCustomizeScalars({
      "home-button_move_nav-bar-end_nav-bar-start_drag": 1,
      "library-button_move_nav-bar-end_bookmarks-bar_drag": 1,
      "stop-reload-button_move_nav-bar-start_tabs-bar_drag": 2,
      "stop-reload-button_move_tabs-bar_nav-bar-start_drag": 1,
      [bookmarksBarTelemetryScalar]: 1,
    });

    CustomizableUI.reset();
  }
});

add_task(async function contextMenus() {
  for (let bookmarksFeatureEnabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.toolbars.bookmarks.2h2020", bookmarksFeatureEnabled]],
    });

    // Create a default state.
    organizeToolbars({
      PersonalToolbar: ["personal-bookmarks"],

      TabsToolbar: ["tabbrowser-tabs", "new-tab-button"],

      "nav-bar": [
        "back-button",
        "forward-button",
        "stop-reload-button",
        "urlbar-container",
        "home-button",
        "library-button",
      ],
    });

    BrowserUsageTelemetry._recordUITelemetry();

    assertVisibilityScalars([
      "menu-toolbar_pinned_off",
      "titlebar_pinned_off",
      "bookmarks-bar_pinned_off",

      "tabbrowser-tabs_pinned_tabs-bar",
      "new-tab-button_pinned_tabs-bar",
      "alltabs-button_pinned_tabs-bar",

      "back-button_pinned_nav-bar-start",
      "forward-button_pinned_nav-bar-start",
      "stop-reload-button_pinned_nav-bar-start",
      "home-button_pinned_nav-bar-end",
      "library-button_pinned_nav-bar-end",

      "personal-bookmarks_pinned_bookmarks-bar",
    ]);

    let menu = document.getElementById("toolbar-context-menu");
    let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
    let button = document.getElementById("stop-reload-button");
    EventUtils.synthesizeMouseAtCenter(
      button,
      { type: "contextmenu", button: 2 },
      window
    );
    await popupShown;

    let barMenu = document.getElementById("toggle_PersonalToolbar");
    let popupHidden = BrowserTestUtils.waitForEvent(menu, "popuphidden");
    if (bookmarksFeatureEnabled) {
      let subMenu = barMenu.querySelector("menupopup");
      popupShown = BrowserTestUtils.waitForEvent(subMenu, "popupshown");
      EventUtils.synthesizeMouseAtCenter(barMenu, {});
      await popupShown;
      let alwaysButton = barMenu.querySelector(
        '*[data-visibility-enum="always"]'
      );
      EventUtils.synthesizeMouseAtCenter(alwaysButton, {});
    } else {
      EventUtils.synthesizeMouseAtCenter(barMenu, {});
    }
    await popupHidden;

    popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(
      button,
      { type: "contextmenu", button: 2 },
      window
    );
    await popupShown;

    popupHidden = BrowserTestUtils.waitForEvent(menu, "popuphidden");
    let removeButton = document.querySelector(
      "#toolbar-context-menu .customize-context-removeFromToolbar"
    );
    EventUtils.synthesizeMouseAtCenter(removeButton, {}, window);
    await popupHidden;

    let bookmarksBarTelemetryScalar = bookmarksFeatureEnabled
      ? "bookmarks-bar_move_off_always_toolbar-context-menu"
      : "bookmarks-bar_move_off_on_toolbar-context-menu";
    assertCustomizeScalars({
      [bookmarksBarTelemetryScalar]: 1,
      "stop-reload-button_remove_nav-bar-start_na_toolbar-context-menu": 1,
    });

    CustomizableUI.reset();
  }
});

add_task(async function pageActions() {
  // Built-in page actions are removed in Proton.
  if (gProton) {
    return;
  }

  // The page action button is only visible when a page is loaded.
  await BrowserTestUtils.withNewTab("http://example.com", async () => {
    // Create a default state.
    organizeToolbars({
      PersonalToolbar: ["personal-bookmarks"],

      TabsToolbar: ["tabbrowser-tabs", "new-tab-button"],

      "nav-bar": [
        "back-button",
        "forward-button",
        "stop-reload-button",
        "urlbar-container",
        "home-button",
        "library-button",
      ],

      pageActionsInUrlBar: ["emailLink", "pinTab"],
    });

    BrowserUsageTelemetry._recordUITelemetry();

    assertVisibilityScalars([
      "menu-toolbar_pinned_off",
      "titlebar_pinned_off",
      "bookmarks-bar_pinned_off",

      "tabbrowser-tabs_pinned_tabs-bar",
      "new-tab-button_pinned_tabs-bar",
      "alltabs-button_pinned_tabs-bar",

      "back-button_pinned_nav-bar-start",
      "forward-button_pinned_nav-bar-start",
      "stop-reload-button_pinned_nav-bar-start",
      "home-button_pinned_nav-bar-end",
      "library-button_pinned_nav-bar-end",

      "emailLink_pinned_pageaction-urlbar",
      "pinTab_pinned_pageaction-urlbar",

      "personal-bookmarks_pinned_bookmarks-bar",
    ]);

    let button = document.getElementById("pageActionButton");
    let context = document.getElementById("pageActionContextMenu");

    EventUtils.synthesizeMouseAtCenter(button, {}, window);
    let panel = document.getElementById("pageActionPanel");
    let popupShown = BrowserTestUtils.waitForEvent(panel, "popupshown");
    await popupShown;

    popupShown = BrowserTestUtils.waitForEvent(context, "popupshown");
    EventUtils.synthesizeMouseAtCenter(
      document.getElementById("pageAction-panel-copyURL"),
      { type: "contextmenu", button: 2 },
      window
    );
    await popupShown;

    let popupHidden = BrowserTestUtils.waitForEvent(context, "popuphidden");
    EventUtils.synthesizeMouseAtCenter(
      document.querySelector(".pageActionContextMenuItem.builtInUnpinned"),
      {},
      window
    );
    await popupHidden;

    popupHidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
    EventUtils.synthesizeMouseAtCenter(button, {}, window);
    await popupHidden;

    popupShown = BrowserTestUtils.waitForEvent(context, "popupshown");
    EventUtils.synthesizeMouseAtCenter(
      document.getElementById("pageAction-urlbar-emailLink"),
      { type: "contextmenu", button: 2 },
      window
    );
    await popupShown;

    popupHidden = BrowserTestUtils.waitForEvent(context, "popuphidden");
    EventUtils.synthesizeMouseAtCenter(
      document.querySelector(".pageActionContextMenuItem.builtInPinned"),
      {},
      window
    );
    await popupHidden;

    assertCustomizeScalars({
      "copyURL_add_na_pageaction-urlbar_pageaction-context": 1,
      "emailLink_remove_pageaction-urlbar_na_pageaction-context": 1,
    });

    CustomizableUI.reset();
  });
});

add_task(async function extensions() {
  // The page action button is only visible when a page is loaded.
  await BrowserTestUtils.withNewTab("http://example.com", async () => {
    organizeToolbars();

    const extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        version: "1",
        applications: {
          gecko: { id: "random_addon@example.com" },
        },
        browser_action: {
          default_icon: "default.png",
          default_title: "Hello",
        },
        page_action: {
          default_icon: "default.png",
          default_title: "Hello",
        },
      },
    });

    await extension.startup();

    assertCustomizeScalars({
      "random-addon-example-com_add_na_nav-bar-end_addon": 1,
      "random-addon-example-com_add_na_pageaction-urlbar_addon": 1,
    });

    BrowserUsageTelemetry._recordUITelemetry();

    assertVisibilityScalars([
      "menu-toolbar_pinned_off",
      "titlebar_pinned_off",
      "bookmarks-bar_pinned_off",

      "tabbrowser-tabs_pinned_tabs-bar",
      "alltabs-button_pinned_tabs-bar",

      "forward-button_pinned_nav-bar-start",
      "back-button_pinned_nav-bar-start",

      "random-addon-example-com_pinned_nav-bar-end",

      "random-addon-example-com_pinned_pageaction-urlbar",
    ]);

    let addon = await AddonManager.getAddonByID(extension.id);
    await addon.disable();

    assertCustomizeScalars({
      "random-addon-example-com_remove_nav-bar-end_na_addon": 1,
      "random-addon-example-com_remove_pageaction-urlbar_na_addon": 1,
    });

    BrowserUsageTelemetry._recordUITelemetry();

    assertVisibilityScalars([
      "menu-toolbar_pinned_off",
      "titlebar_pinned_off",
      "bookmarks-bar_pinned_off",

      "tabbrowser-tabs_pinned_tabs-bar",
      "alltabs-button_pinned_tabs-bar",

      "forward-button_pinned_nav-bar-start",
      "back-button_pinned_nav-bar-start",
    ]);

    await addon.enable();

    assertCustomizeScalars({
      "random-addon-example-com_add_na_nav-bar-end_addon": 1,
      "random-addon-example-com_add_na_pageaction-urlbar_addon": 1,
    });

    BrowserUsageTelemetry._recordUITelemetry();

    assertVisibilityScalars([
      "menu-toolbar_pinned_off",
      "titlebar_pinned_off",
      "bookmarks-bar_pinned_off",

      "tabbrowser-tabs_pinned_tabs-bar",
      "alltabs-button_pinned_tabs-bar",

      "forward-button_pinned_nav-bar-start",
      "back-button_pinned_nav-bar-start",

      "random-addon-example-com_pinned_nav-bar-end",

      "random-addon-example-com_pinned_pageaction-urlbar",
    ]);

    await addon.reload();

    assertCustomizeScalars({});

    await enterCustomizationMode();

    let navbar = CustomizableUI.getCustomizationTarget(
      document.getElementById("nav-bar")
    );

    simulateItemDrag(
      document.getElementById("random_addon_example_com-browser-action"),
      navbar,
      "start"
    );

    await leaveCustomizationMode();

    assertCustomizeScalars({
      "random-addon-example-com_move_nav-bar-end_nav-bar-start_drag": 1,
    });

    await extension.unload();

    assertCustomizeScalars({
      "random-addon-example-com_remove_nav-bar-start_na_addon": 1,
      "random-addon-example-com_remove_pageaction-urlbar_na_addon": 1,
    });

    BrowserUsageTelemetry._recordUITelemetry();

    assertVisibilityScalars([
      "menu-toolbar_pinned_off",
      "titlebar_pinned_off",
      "bookmarks-bar_pinned_off",

      "tabbrowser-tabs_pinned_tabs-bar",
      "alltabs-button_pinned_tabs-bar",

      "forward-button_pinned_nav-bar-start",
      "back-button_pinned_nav-bar-start",
    ]);
  });
});
