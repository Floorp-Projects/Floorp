/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kDevPanelID =
  gProton && gProtonDoorhangers ? "appmenu-moreTools" : "PanelUI-developer";

/**
 * Test the behavior of key presses on various toolbar buttons.
 */

function waitForLocationChange() {
  let promise = new Promise(resolve => {
    let wpl = {
      onLocationChange(aWebProgress, aRequest, aLocation) {
        gBrowser.removeProgressListener(wpl);
        resolve();
      },
    };
    gBrowser.addProgressListener(wpl);
  });
  return promise;
}

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.keyboard_navigation", true]],
  });
});

// Test activation of the app menu button from the keyboard.
// The app menu should appear and focus should move inside it.
add_task(async function testAppMenuButtonPress() {
  let button = document.getElementById("PanelUI-menu-button");
  forceFocus(button);
  let focused = BrowserTestUtils.waitForEvent(
    window.PanelUI.mainView,
    "focus",
    true
  );
  EventUtils.synthesizeKey(" ");
  await focused;
  ok(true, "Focus inside app menu after toolbar button pressed");
  let hidden = BrowserTestUtils.waitForEvent(
    window.PanelUI.panel,
    "popuphidden"
  );
  EventUtils.synthesizeKey("KEY_Escape");
  await hidden;
});

// Test that the app menu doesn't open when a key other than space or enter is
// pressed .
add_task(async function testAppMenuButtonWrongKey() {
  let button = document.getElementById("PanelUI-menu-button");
  forceFocus(button);
  EventUtils.synthesizeKey("KEY_Tab");
  await TestUtils.waitForTick();
  is(window.PanelUI.panel.state, "closed", "App menu is closed after tab");
});

// Test activation of the Library button from the keyboard.
// The Library menu should appear and focus should move inside it.
add_task(async function testLibraryButtonPress() {
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea("library-button", "nav-bar");
  }
  let button = document.getElementById("library-button");
  forceFocus(button);
  EventUtils.synthesizeKey(" ");
  let view = document.getElementById("appMenu-libraryView");
  let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
  await focused;
  ok(true, "Focus inside Library menu after toolbar button pressed");
  let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
  view.closest("panel").hidePopup();
  await hidden;
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.removeWidgetFromArea("library-button");
  }
});

// Test activation of the Developer button from the keyboard.
// This is a customizable widget of type "view".
// The Developer menu should appear and focus should move inside it.
add_task(async function testDeveloperButtonPress() {
  CustomizableUI.addWidgetToArea(
    "developer-button",
    CustomizableUI.AREA_NAVBAR
  );
  let button = document.getElementById("developer-button");
  forceFocus(button);
  EventUtils.synthesizeKey(" ");
  let view = document.getElementById(kDevPanelID);
  let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
  await focused;
  ok(true, "Focus inside Developer menu after toolbar button pressed");
  let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
  view.closest("panel").hidePopup();
  await hidden;
  CustomizableUI.reset();
});

// Test that the Developer menu doesn't open when a key other than space or
// enter is pressed .
add_task(async function testDeveloperButtonWrongKey() {
  CustomizableUI.addWidgetToArea(
    "developer-button",
    CustomizableUI.AREA_NAVBAR
  );
  let button = document.getElementById("developer-button");
  forceFocus(button);
  EventUtils.synthesizeKey("KEY_Tab");
  await TestUtils.waitForTick();
  let panel = document.getElementById(kDevPanelID).closest("panel");
  ok(!panel || panel.state == "closed", "Developer menu not open after tab");
  CustomizableUI.reset();
});

// Test activation of the Page actions button from the keyboard.
// The Page Actions menu should appear and focus should move inside it.
add_task(async function testPageActionsButtonPress() {
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    let button = document.getElementById("pageActionButton");
    forceFocus(button);
    EventUtils.synthesizeKey(" ");
    let view = document.getElementById("pageActionPanelMainView");
    let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
    await focused;
    ok(true, "Focus inside Page Actions menu after toolbar button pressed");
    let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
    view.closest("panel").hidePopup();
    await hidden;
  });
});

// Test activation of the Back and Forward buttons from the keyboard.
add_task(async function testBackForwardButtonPress() {
  await BrowserTestUtils.withNewTab("https://example.com/1", async function(
    aBrowser
  ) {
    BrowserTestUtils.loadURI(aBrowser, "https://example.com/2");

    await BrowserTestUtils.browserLoaded(aBrowser);
    let backButton = document.getElementById("back-button");
    forceFocus(backButton);
    let onLocationChange = waitForLocationChange();
    EventUtils.synthesizeKey(" ");
    await onLocationChange;
    ok(true, "Location changed after back button pressed");

    let forwardButton = document.getElementById("forward-button");
    forceFocus(forwardButton);
    onLocationChange = waitForLocationChange();
    EventUtils.synthesizeKey(" ");
    await onLocationChange;
    ok(true, "Location changed after forward button pressed");
  });
});

// Test activation of the Send Tab to Device button from the keyboard.
// This is a page action button built at runtime by PageActions.
// The Send Tab to Device menu should appear and focus should move inside it.
add_task(async function testSendTabToDeviceButtonPress() {
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    PageActions.actionForID("sendToDevice").pinnedToUrlbar = true;
    let button = document.getElementById("pageAction-urlbar-sendToDevice");
    forceFocus(button);
    let mainPopupSet = document.getElementById("mainPopupSet");
    let focused = BrowserTestUtils.waitForEvent(mainPopupSet, "focus", true);
    EventUtils.synthesizeKey(" ");
    await focused;
    let view = document.getElementById(
      "pageAction-urlbar-sendToDevice-subview"
    );
    ok(
      view.contains(document.activeElement),
      "Focus inside Page Actions menu after toolbar button pressed"
    );
    let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
    view.closest("panel").hidePopup();
    await hidden;
    PageActions.actionForID("sendToDevice").pinnedToUrlbar = false;
  });
});

// Test activation of the Reload button from the keyboard.
// This is a toolbarbutton with a click handler and no command handler, but
// the toolbar keyboard navigation code should handle keyboard activation.
add_task(async function testReloadButtonPress() {
  await BrowserTestUtils.withNewTab("https://example.com", async function(
    aBrowser
  ) {
    let button = document.getElementById("reload-button");
    await TestUtils.waitForCondition(() => !button.disabled);
    forceFocus(button);
    let loaded = BrowserTestUtils.browserLoaded(aBrowser);
    EventUtils.synthesizeKey(" ");
    await loaded;
    ok(true, "Page loaded after Reload button pressed");
  });
});

// Test activation of the Sidebars button from the keyboard.
// This is a toolbarbutton with a command handler.
add_task(async function testSidebarsButtonPress() {
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar");
  }
  let button = document.getElementById("sidebar-button");
  ok(!button.checked, "Sidebars button not checked at start of test");
  let sidebarBox = document.getElementById("sidebar-box");
  ok(sidebarBox.hidden, "Sidebar hidden at start of test");
  forceFocus(button);
  EventUtils.synthesizeKey(" ");
  await TestUtils.waitForCondition(() => button.checked);
  ok(true, "Sidebars button checked after press");
  ok(!sidebarBox.hidden, "Sidebar visible after press");
  // Make sure the sidebar is fully loaded before we hide it.
  // Otherwise, the unload event might call JS which isn't loaded yet.
  // We can't use BrowserTestUtils.browserLoaded because it fails on non-tab
  // docs. Instead, wait for something in the JS script.
  let sidebarWin = document.getElementById("sidebar").contentWindow;
  await TestUtils.waitForCondition(() => sidebarWin.PlacesUIUtils);
  forceFocus(button);
  EventUtils.synthesizeKey(" ");
  await TestUtils.waitForCondition(() => !button.checked);
  ok(true, "Sidebars button not checked after press");
  ok(sidebarBox.hidden, "Sidebar hidden after press");
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.removeWidgetFromArea("sidebar-button");
  }
});

// Test activation of the Bookmark this page button from the keyboard.
// This is an image with a click handler on its parent and no command handler,
// but the toolbar keyboard navigation code should handle keyboard activation.
add_task(async function testBookmarkButtonPress() {
  await BrowserTestUtils.withNewTab("https://example.com", async function(
    aBrowser
  ) {
    let button = document.getElementById("star-button");
    forceFocus(button);
    StarUI._createPanelIfNeeded();
    let panel = document.getElementById("editBookmarkPanel");
    let focused = BrowserTestUtils.waitForEvent(panel, "focus", true);
    // The button ignores activation while the bookmarked status is being
    // updated. So, wait for it to finish updating.
    await TestUtils.waitForCondition(
      () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING
    );
    EventUtils.synthesizeKey(" ");
    await focused;
    ok(true, "Focus inside edit bookmark panel after Bookmark button pressed");
    let hidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
    EventUtils.synthesizeKey("KEY_Escape");
    await hidden;
  });
});

// Test activation of the Bookmarks Menu button from the keyboard.
// This is a button with type="menu".
// The Bookmarks Menu should appear.
add_task(async function testBookmarksmenuButtonPress() {
  CustomizableUI.addWidgetToArea(
    "bookmarks-menu-button",
    CustomizableUI.AREA_NAVBAR
  );
  let button = document.getElementById("bookmarks-menu-button");
  forceFocus(button);
  let menu = document.getElementById("BMB_bookmarksPopup");
  let shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeKey(" ");
  await shown;
  ok(true, "Bookmarks Menu shown after toolbar button pressed");
  let hidden = BrowserTestUtils.waitForEvent(menu, "popuphidden");
  menu.hidePopup();
  await hidden;
  CustomizableUI.reset();
});

// Test activation of the overflow button from the keyboard.
// The overflow menu should appear and focus should move inside it.
add_task(async function testOverflowButtonPress() {
  // Move something to the overflow menu to make the button appear.
  CustomizableUI.addWidgetToArea(
    "developer-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  let button = document.getElementById("nav-bar-overflow-button");
  forceFocus(button);
  let view = document.getElementById("widget-overflow-mainView");
  let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
  EventUtils.synthesizeKey(" ");
  await focused;
  ok(true, "Focus inside overflow menu after toolbar button pressed");
  let panel = document.getElementById("widget-overflow");
  let hidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
  panel.hidePopup();
  await hidden;
  CustomizableUI.reset();
});

// Test activation of the Downloads button from the keyboard.
// The Downloads panel should appear and focus should move inside it.
add_task(async function testDownloadsButtonPress() {
  DownloadsButton.unhide();
  let button = document.getElementById("downloads-button");
  forceFocus(button);
  let panel = document.getElementById("downloadsPanel");
  let focused = BrowserTestUtils.waitForEvent(panel, "focus", true);
  EventUtils.synthesizeKey(" ");
  await focused;
  ok(true, "Focus inside Downloads panel after toolbar button pressed");
  let hidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
  panel.hidePopup();
  await hidden;
  DownloadsButton.hide();
});

// Test activation of the Save to Pocket button from the keyboard.
// This is a customizable widget button which shows an iframe.
// The Pocket iframe should appear and focus should move inside it.
add_task(async function testPocketButtonPress() {
  await BrowserTestUtils.withNewTab("https://example.com", async function(
    aBrowser
  ) {
    let button = document.getElementById("save-to-pocket-button");
    forceFocus(button);
    // The panel is created on the fly, so we can't simply wait for focus
    // inside it.
    let showing = BrowserTestUtils.waitForEvent(document, "popupshowing", true);
    EventUtils.synthesizeKey(" ");
    let event = await showing;
    let panel = event.target;
    is(panel.id, "customizationui-widget-panel");
    let focused = BrowserTestUtils.waitForEvent(panel, "focus", true);
    await focused;
    is(
      document.activeElement.tagName,
      "iframe",
      "Focus inside Pocket iframe after Bookmark button pressed"
    );
    let hidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
    EventUtils.synthesizeKey("KEY_Escape");
    await hidden;
  });
});
