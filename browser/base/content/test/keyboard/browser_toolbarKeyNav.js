/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test browser toolbar keyboard navigation.
 * These tests assume the default browser configuration for toolbars unless
 * otherwise specified.
 */

const PERMISSIONS_PAGE =
  "https://example.com/browser/browser/base/content/test/permissions/permissions.html";
const afterUrlBarButton = "save-to-pocket-button";

// The DevEdition has the DevTools button in the toolbar by default. Remove it
// to prevent branch-specific rules what button should be focused.
function resetToolbarWithoutDevEditionButtons() {
  CustomizableUI.reset();
  CustomizableUI.removeWidgetFromArea("developer-button");
}

function AddHomeBesideReload() {
  CustomizableUI.addWidgetToArea(
    "home-button",
    "nav-bar",
    CustomizableUI.getPlacementOfWidget("stop-reload-button").position + 1
  );
}

function RemoveHomeButton() {
  CustomizableUI.removeWidgetFromArea("home-button");
}

function AddOldMenuSideButtons() {
  // Make the FxA button visible even though we're signed out.
  // We'll use oldfxastatus to restore the old state.
  document.documentElement.setAttribute(
    "oldfxastatus",
    document.documentElement.getAttribute("fxastatus")
  );
  document.documentElement.setAttribute("fxastatus", "signed_in");
  // The FxA button is supposed to be last, add these buttons before it.
  CustomizableUI.addWidgetToArea(
    "library-button",
    "nav-bar",
    CustomizableUI.getWidgetIdsInArea("nav-bar").length - 3
  );
  CustomizableUI.addWidgetToArea(
    "sidebar-button",
    "nav-bar",
    CustomizableUI.getWidgetIdsInArea("nav-bar").length - 3
  );
  CustomizableUI.addWidgetToArea(
    "unified-extensions-button",
    "nav-bar",
    CustomizableUI.getWidgetIdsInArea("nav-bar").length - 3
  );
}

function RemoveOldMenuSideButtons() {
  CustomizableUI.removeWidgetFromArea("library-button");
  CustomizableUI.removeWidgetFromArea("sidebar-button");
  document.documentElement.setAttribute(
    "fxastatus",
    document.documentElement.getAttribute("oldfxastatus")
  );
  document.documentElement.removeAttribute("oldfxastatus");
}

function startFromUrlBar(aWindow = window) {
  aWindow.gURLBar.focus();
  is(
    aWindow.document.activeElement,
    aWindow.gURLBar.inputField,
    "URL bar focused for start of test"
  );
}

// The Reload button is disabled for a short time even after the page finishes
// loading. Wait for it to be enabled.
async function waitUntilReloadEnabled() {
  let button = document.getElementById("reload-button");
  await TestUtils.waitForCondition(() => !button.disabled);
}

// Opens a new, blank tab, executes a task and closes the tab.
function withNewBlankTab(taskFn) {
  return BrowserTestUtils.withNewTab("about:blank", async function () {
    // For a blank tab, the Reload button should be disabled. However, when we
    // open about:blank with BrowserTestUtils.withNewTab, this is unreliable.
    // Therefore, explicitly disable the reload command.
    // We disable the command (rather than disabling the button directly) so the
    // button will be updated correctly for future page loads.
    document.getElementById("Browser:Reload").setAttribute("disabled", "true");
    await taskFn();
  });
}

function removeFirefoxViewButton() {
  CustomizableUI.removeWidgetFromArea("firefox-view-button");
}

const BOOKMARKS_COUNT = 100;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.toolbars.keyboard_navigation", true],
      ["accessibility.tabfocus", 7],
    ],
  });
  resetToolbarWithoutDevEditionButtons();

  await PlacesUtils.bookmarks.eraseEverything();
  // Add bookmarks.
  let bookmarks = new Array(BOOKMARKS_COUNT);
  for (let i = 0; i < BOOKMARKS_COUNT; ++i) {
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    bookmarks[i] = { url: `http://test.places.${i}y/` };
  }
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: bookmarks,
  });

  // The page actions button is not normally visible, so we must
  // unhide it.
  BrowserPageActions.mainButtonNode.style.visibility = "visible";
  registerCleanupFunction(() => {
    BrowserPageActions.mainButtonNode.style.removeProperty("visibility");
  });
});

// Test tab stops with no page loaded.
add_task(async function testTabStopsNoPageWithHomeButton() {
  AddHomeBesideReload();
  await withNewBlankTab(async function () {
    startFromUrlBar();
    await expectFocusAfterKey("Shift+Tab", "home-button");
    await expectFocusAfterKey("Shift+Tab", "tabs-newtab-button");
    await expectFocusAfterKey("Shift+Tab", gBrowser.selectedTab);
    await expectFocusAfterKey("Tab", "tabs-newtab-button");
    await expectFocusAfterKey("Tab", "home-button");
    await expectFocusAfterKey("Tab", gURLBar.inputField);
    await expectFocusAfterKey("Tab", afterUrlBarButton);
    await expectFocusAfterKey("Tab", gBrowser.selectedBrowser);
  });
  RemoveHomeButton();
});

async function doTestTabStopsPageLoaded(aPageActionsVisible) {
  info(`doTestTabStopsPageLoaded(${aPageActionsVisible})`);

  BrowserPageActions.mainButtonNode.style.visibility = aPageActionsVisible
    ? "visible"
    : "";
  await BrowserTestUtils.withNewTab("https://example.com", async function () {
    await waitUntilReloadEnabled();
    startFromUrlBar();
    await expectFocusAfterKey(
      "Shift+Tab",
      "tracking-protection-icon-container"
    );
    await expectFocusAfterKey("Shift+Tab", "reload-button");
    await expectFocusAfterKey("Shift+Tab", "tabs-newtab-button");
    await expectFocusAfterKey("Shift+Tab", gBrowser.selectedTab);
    await expectFocusAfterKey("Tab", "tabs-newtab-button");
    await expectFocusAfterKey("Tab", "reload-button");
    await expectFocusAfterKey("Tab", "tracking-protection-icon-container");
    await expectFocusAfterKey("Tab", gURLBar.inputField);
    await expectFocusAfterKey(
      "Tab",
      aPageActionsVisible ? "pageActionButton" : "star-button-box"
    );
    await expectFocusAfterKey("Tab", afterUrlBarButton);
    await expectFocusAfterKey("Tab", gBrowser.selectedBrowser);
  });
}

// Test tab stops with a page loaded.
add_task(async function testTabStopsPageLoaded() {
  is(
    BrowserPageActions.mainButtonNode.style.visibility,
    "visible",
    "explicitly shown at the beginning of test"
  );
  await doTestTabStopsPageLoaded(false);
  await doTestTabStopsPageLoaded(true);
});

// Test tab stops with a notification anchor visible.
// The notification anchor should not get its own tab stop.
add_task(async function testTabStopsWithNotification() {
  await BrowserTestUtils.withNewTab(
    PERMISSIONS_PAGE,
    async function (aBrowser) {
      let popupShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      // Request a permission.
      BrowserTestUtils.synthesizeMouseAtCenter("#geo", {}, aBrowser);
      await popupShown;
      startFromUrlBar();
      // If the notification anchor were in the tab order, the next shift+tab
      // would focus it instead of #tracking-protection-icon-container.
      await expectFocusAfterKey(
        "Shift+Tab",
        "tracking-protection-icon-container"
      );
    }
  );
});

// Test tab stops with the Bookmarks toolbar visible.
add_task(async function testTabStopsWithBookmarksToolbar() {
  await BrowserTestUtils.withNewTab("about:blank", async function () {
    CustomizableUI.setToolbarVisibility("PersonalToolbar", true);
    startFromUrlBar();
    await expectFocusAfterKey("Tab", afterUrlBarButton);
    await expectFocusAfterKey("Tab", "PersonalToolbar", true);
    await expectFocusAfterKey("Tab", gBrowser.selectedBrowser);

    // Make sure the Bookmarks toolbar is no longer tabbable once hidden.
    CustomizableUI.setToolbarVisibility("PersonalToolbar", false);
    startFromUrlBar();
    await expectFocusAfterKey("Tab", afterUrlBarButton);
    await expectFocusAfterKey("Tab", gBrowser.selectedBrowser);
  });
});

// Test a focusable toolbartabstop which has no navigable buttons.
add_task(async function testTabStopNoButtons() {
  await withNewBlankTab(async function () {
    // The Back, Forward and Reload buttons are all currently disabled.
    // The Home button is the only other button at that tab stop.
    CustomizableUI.removeWidgetFromArea("home-button");
    startFromUrlBar();
    await expectFocusAfterKey("Shift+Tab", "tabs-newtab-button");
    await expectFocusAfterKey("Tab", gURLBar.inputField);
    resetToolbarWithoutDevEditionButtons();
    AddHomeBesideReload();
    // Make sure the button is reachable now that it has been re-added.
    await expectFocusAfterKey("Shift+Tab", "home-button", true);
    RemoveHomeButton();
  });
});

// Test that right/left arrows move through toolbarbuttons.
// This also verifies that:
// 1. Right/left arrows do nothing when at the edges; and
// 2. The overflow menu button can't be reached by right arrow when it isn't
// visible.
add_task(async function testArrowsToolbarbuttons() {
  AddOldMenuSideButtons();
  await BrowserTestUtils.withNewTab("about:blank", async function () {
    startFromUrlBar();
    await expectFocusAfterKey("Tab", afterUrlBarButton);
    EventUtils.synthesizeKey("KEY_ArrowLeft");
    is(
      document.activeElement.id,
      afterUrlBarButton,
      "ArrowLeft at end of button group does nothing"
    );
    await expectFocusAfterKey("ArrowRight", "library-button");
    await expectFocusAfterKey("ArrowRight", "sidebar-button");
    await expectFocusAfterKey("ArrowRight", "unified-extensions-button");
    await expectFocusAfterKey("ArrowRight", "fxa-toolbar-menu-button");
    // This next check also confirms that the overflow menu button is skipped,
    // since it is currently invisible.
    await expectFocusAfterKey("ArrowRight", "PanelUI-menu-button");
    EventUtils.synthesizeKey("KEY_ArrowRight");
    is(
      document.activeElement.id,
      "PanelUI-menu-button",
      "ArrowRight at end of button group does nothing"
    );
    await expectFocusAfterKey("ArrowLeft", "fxa-toolbar-menu-button");
    await expectFocusAfterKey("ArrowLeft", "unified-extensions-button");
    await expectFocusAfterKey("ArrowLeft", "sidebar-button");
    await expectFocusAfterKey("ArrowLeft", "library-button");
  });
  RemoveOldMenuSideButtons();
});

// Test that right/left arrows move through buttons which aren't toolbarbuttons
// but have role="button".
add_task(async function testArrowsRoleButton() {
  await BrowserTestUtils.withNewTab("https://example.com", async function () {
    startFromUrlBar();
    await expectFocusAfterKey("Tab", "pageActionButton");
    await expectFocusAfterKey("ArrowRight", "star-button-box");
    await expectFocusAfterKey("ArrowLeft", "pageActionButton");
  });
});

// Test that right/left arrows do not land on disabled buttons.
add_task(async function testArrowsDisabledButtons() {
  await BrowserTestUtils.withNewTab(
    "https://example.com",
    async function (aBrowser) {
      await waitUntilReloadEnabled();
      startFromUrlBar();
      await expectFocusAfterKey(
        "Shift+Tab",
        "tracking-protection-icon-container"
      );
      // Back and Forward buttons are disabled.
      await expectFocusAfterKey("Shift+Tab", "reload-button");
      EventUtils.synthesizeKey("KEY_ArrowLeft");
      is(
        document.activeElement.id,
        "reload-button",
        "ArrowLeft on Reload button when prior buttons disabled does nothing"
      );

      BrowserTestUtils.startLoadingURIString(aBrowser, "https://example.com/2");
      await BrowserTestUtils.browserLoaded(aBrowser);
      await waitUntilReloadEnabled();
      startFromUrlBar();
      await expectFocusAfterKey(
        "Shift+Tab",
        "tracking-protection-icon-container"
      );
      await expectFocusAfterKey("Shift+Tab", "back-button");
      // Forward button is still disabled.
      await expectFocusAfterKey("ArrowRight", "reload-button");
    }
  );
});

// Test that right arrow reaches the overflow menu button when it is visible.
add_task(async function testArrowsOverflowButton() {
  AddOldMenuSideButtons();
  await BrowserTestUtils.withNewTab("about:blank", async function () {
    // Move something to the overflow menu to make the button appear.
    CustomizableUI.addWidgetToArea(
      "home-button",
      CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
    );
    startFromUrlBar();
    await expectFocusAfterKey("Tab", afterUrlBarButton);
    await expectFocusAfterKey("ArrowRight", "library-button");
    await expectFocusAfterKey("ArrowRight", "sidebar-button");
    await expectFocusAfterKey("ArrowRight", "unified-extensions-button");
    await expectFocusAfterKey("ArrowRight", "fxa-toolbar-menu-button");
    await expectFocusAfterKey("ArrowRight", "nav-bar-overflow-button");
    // Make sure the button is not reachable once it is invisible again.
    await expectFocusAfterKey("ArrowRight", "PanelUI-menu-button");
    resetToolbarWithoutDevEditionButtons();
    // Flush layout so its invisibility can be detected.
    document.getElementById("nav-bar-overflow-button").clientWidth;
    // We reset the toolbar above so the unified extensions button is now the
    // "last" button.
    await expectFocusAfterKey("ArrowLeft", "unified-extensions-button");
    await expectFocusAfterKey("ArrowLeft", "fxa-toolbar-menu-button");
  });
  RemoveOldMenuSideButtons();
});

// Test that toolbar keyboard navigation doesn't interfere with PanelMultiView
// keyboard navigation.
// We do this by opening the Library menu and ensuring that pressing left arrow
// does nothing.
add_task(async function testArrowsInPanelMultiView() {
  AddOldMenuSideButtons();
  let button = document.getElementById("library-button");
  await focusAndActivateElement(button, () => EventUtils.synthesizeKey(" "));
  let view = document.getElementById("appMenu-libraryView");
  let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
  let focusEvt = await focused;
  ok(true, "Focus inside Library menu after toolbar button pressed");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  is(
    document.activeElement,
    focusEvt.target,
    "ArrowLeft inside panel does nothing"
  );
  let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
  view.closest("panel").hidePopup();
  await hidden;
  RemoveOldMenuSideButtons();
});

// Test that right/left arrows move in the expected direction for RTL locales.
add_task(async function testArrowsRtl() {
  AddOldMenuSideButtons();
  await SpecialPowers.pushPrefEnv({ set: [["intl.l10n.pseudo", "bidi"]] });
  // window.RTL_UI doesn't update in existing windows when this pref is changed,
  // so we need to test in a new window.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  startFromUrlBar(win);
  await expectFocusAfterKey("Tab", afterUrlBarButton, false, win);
  EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
  is(
    win.document.activeElement.id,
    afterUrlBarButton,
    "ArrowRight at end of button group does nothing"
  );
  await expectFocusAfterKey("ArrowLeft", "library-button", false, win);
  await expectFocusAfterKey("ArrowLeft", "sidebar-button", false, win);
  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
  RemoveOldMenuSideButtons();
});

// Test that right arrow reaches the overflow menu button on the Bookmarks
// toolbar when it is visible.
add_task(async function testArrowsBookmarksOverflowButton() {
  let toolbar = gNavToolbox.querySelector("#PersonalToolbar");
  // Third parameter is 'persist' and true is the default.
  // Fourth parameter is 'animated' and we want no animation.
  setToolbarVisibility(toolbar, true, true, false);
  Assert.ok(!toolbar.collapsed, "toolbar should be visible");

  await BrowserTestUtils.waitForEvent(
    toolbar,
    "BookmarksToolbarVisibilityUpdated"
  );
  let items = document.getElementById("PlacesToolbarItems").children;
  let lastVisible;
  for (let item of items) {
    if (item.style.visibility == "hidden") {
      break;
    }
    lastVisible = item;
  }
  await focusAndActivateElement(lastVisible, () =>
    expectFocusAfterKey("ArrowRight", "PlacesChevron")
  );
  setToolbarVisibility(toolbar, false, true, false);
});

registerCleanupFunction(async function () {
  CustomizableUI.reset();
  await PlacesUtils.bookmarks.eraseEverything();
});

// Test that when a toolbar button opens a panel, closing the panel restores
// focus to the button which opened it.
add_task(async function testPanelCloseRestoresFocus() {
  AddOldMenuSideButtons();
  await withNewBlankTab(async function () {
    // We can't use forceFocus because that removes focusability immediately.
    // Instead, we must let ToolbarKeyboardNavigator handle this properly.
    startFromUrlBar();
    await expectFocusAfterKey("Tab", afterUrlBarButton);
    await expectFocusAfterKey("ArrowRight", "library-button");
    let view = document.getElementById("appMenu-libraryView");
    let shown = BrowserTestUtils.waitForEvent(view, "ViewShown");
    EventUtils.synthesizeKey(" ");
    await shown;
    let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
    view.closest("panel").hidePopup();
    await hidden;
    is(
      document.activeElement.id,
      "library-button",
      "Focus restored to Library button after panel closed"
    );
  });
  RemoveOldMenuSideButtons();
});

// Test that the arrow key works in the group of the
// 'tracking-protection-icon-container' and the 'identity-box'.
add_task(async function testArrowKeyForTPIconContainerandIdentityBox() {
  await BrowserTestUtils.withNewTab(
    "https://example.com",
    async function (browser) {
      // Simulate geo sharing so the permission box shows
      gBrowser.updateBrowserSharing(browser, { geo: true });
      await waitUntilReloadEnabled();
      startFromUrlBar();
      await expectFocusAfterKey(
        "Shift+Tab",
        "tracking-protection-icon-container"
      );
      await expectFocusAfterKey("ArrowRight", "identity-icon-box");
      await expectFocusAfterKey("ArrowRight", "identity-permission-box");
      await expectFocusAfterKey("ArrowLeft", "identity-icon-box");
      await expectFocusAfterKey(
        "ArrowLeft",
        "tracking-protection-icon-container"
      );
      gBrowser.updateBrowserSharing(browser, { geo: false });
    }
  );
});

// Test navigation by typed characters.
add_task(async function testCharacterNavigation() {
  AddHomeBesideReload();
  AddOldMenuSideButtons();
  await BrowserTestUtils.withNewTab("https://example.com", async function () {
    await waitUntilReloadEnabled();
    startFromUrlBar();
    await expectFocusAfterKey("Tab", "pageActionButton");
    await expectFocusAfterKey("h", "home-button");
    // There's no button starting with "hs", so pressing s should do nothing.
    EventUtils.synthesizeKey("s");
    is(
      document.activeElement.id,
      "home-button",
      "home-button still focused after s pressed"
    );
    // Escape should reset the search.
    EventUtils.synthesizeKey("KEY_Escape");
    // Now that the search is reset, pressing s should focus Save to Pocket.
    await expectFocusAfterKey("s", "save-to-pocket-button");
    // Pressing i makes the search "si", so it should focus Sidebars.
    await expectFocusAfterKey("i", "sidebar-button");
    // Reset the search.
    EventUtils.synthesizeKey("KEY_Escape");
    await expectFocusAfterKey("s", "save-to-pocket-button");
    // Pressing s again should find the next button starting with s: Sidebars.
    await expectFocusAfterKey("s", "sidebar-button");
  });
  RemoveHomeButton();
  RemoveOldMenuSideButtons();
});

// Test that toolbar character navigation doesn't trigger in PanelMultiView for
// a panel anchored to the toolbar.
// We do this by opening the Library menu and ensuring that pressing s
// does nothing.
// This test should be removed if PanelMultiView implements character
// navigation.
add_task(async function testCharacterInPanelMultiView() {
  AddOldMenuSideButtons();
  let button = document.getElementById("library-button");
  let view = document.getElementById("appMenu-libraryView");
  let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
  await focusAndActivateElement(button, () => EventUtils.synthesizeKey(" "));
  let focusEvt = await focused;
  ok(true, "Focus inside Library menu after toolbar button pressed");
  EventUtils.synthesizeKey("s");
  is(document.activeElement, focusEvt.target, "s inside panel does nothing");
  let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
  view.closest("panel").hidePopup();
  await hidden;
  RemoveOldMenuSideButtons();
});

// Test tab stops after the search bar is added.
add_task(async function testTabStopsAfterSearchBarAdded() {
  AddOldMenuSideButtons();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.widget.inNavBar", 1]],
  });
  await withNewBlankTab(async function () {
    startFromUrlBar();
    await expectFocusAfterKey("Tab", "searchbar", true);
    await expectFocusAfterKey("Tab", afterUrlBarButton);
    await expectFocusAfterKey("ArrowRight", "library-button");
  });
  await SpecialPowers.popPrefEnv();
  RemoveOldMenuSideButtons();
});

// Test tab navigation when the Firefox View button is present
// and when the button is not present.
add_task(async function testFirefoxViewButtonNavigation() {
  // Add enough tabs so that the new-tab-button appears in the toolbar
  // and the tabs-newtab-button is hidden.
  await BrowserTestUtils.overflowTabs(registerCleanupFunction, window);

  // Assert that Firefox View button receives focus when tab navigating
  // forward from the end of web content.
  // Additionally, ensure that focus is not trapped between the
  // selected tab and the new-tab button.
  // Finally, assert that focus is restored to web content when
  // navigating backwards from the Firefox View button.
  await BrowserTestUtils.withNewTab(
    PERMISSIONS_PAGE,
    async function (aBrowser) {
      await SpecialPowers.spawn(aBrowser, [], async () => {
        content.document.querySelector("#camera").focus();
      });

      await expectFocusAfterKey("Tab", "firefox-view-button");
      let selectedTab = document.querySelector("tab[selected]");
      await expectFocusAfterKey("Tab", selectedTab);
      await expectFocusAfterKey("Tab", "new-tab-button");
      await expectFocusAfterKey("Shift+Tab", selectedTab);
      await expectFocusAfterKey("Shift+Tab", "firefox-view-button");

      // Moving from toolbar back into content
      EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
      await SpecialPowers.spawn(aBrowser, [], async () => {
        let activeElement = content.document.activeElement;
        let expectedElement = content.document.querySelector("#camera");
        is(
          activeElement,
          expectedElement,
          "Focus should be returned to the 'camera' button"
        );
      });
    }
  );

  // Assert that the selected tab receives focus before the new-tab button
  // if there is no Firefox View button.
  // Additionally, assert that navigating backwards from the selected tab
  // restores focus to the last element in the web content.
  await BrowserTestUtils.withNewTab(
    PERMISSIONS_PAGE,
    async function (aBrowser) {
      removeFirefoxViewButton();

      await SpecialPowers.spawn(aBrowser, [], async () => {
        content.document.querySelector("#camera").focus();
      });

      let selectedTab = document.querySelector("tab[selected]");
      await expectFocusAfterKey("Tab", selectedTab);
      await expectFocusAfterKey("Tab", "new-tab-button");
      await expectFocusAfterKey("Shift+Tab", selectedTab);

      // Moving from toolbar back into content
      EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
      await SpecialPowers.spawn(aBrowser, [], async () => {
        let activeElement = content.document.activeElement;
        let expectedElement = content.document.querySelector("#camera");
        is(
          activeElement,
          expectedElement,
          "Focus should be returned to the 'camera' button"
        );
      });
    }
  );

  // Clean up extra tabs
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
  CustomizableUI.reset();
});
