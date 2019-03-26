/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test browser toolbar keyboard navigation.
 * These tests assume the default browser configuration for toolbars unless
 * otherwise specified.
 */

const PERMISSIONS_PAGE = "https://example.com/browser/browser/base/content/test/permissions/permissions.html";

// The DevEdition has the DevTools button in the toolbar by default. Remove it
// to prevent branch-specific rules what button should be focused.
function resetToolbarWithoutDevEditionButtons() {
  CustomizableUI.reset();
  CustomizableUI.removeWidgetFromArea("developer-button");
}

async function expectFocusAfterKey(aKey, aFocus, aAncestorOk = false) {
  let res = aKey.match(/^(Shift\+)?(?:(.)|(.+))$/);
  let shift = Boolean(res[1]);
  let key;
  if (res[2]) {
    key = res[2]; // Character.
  } else {
    key = "KEY_" + res[3]; // Tab, ArrowRight, etc.
  }
  let expected;
  let friendlyExpected;
  if (typeof aFocus == "string") {
    expected = document.getElementById(aFocus);
    friendlyExpected = aFocus;
  } else {
    expected = aFocus;
    if (aFocus == gURLBar.inputField) {
      friendlyExpected = "URL bar input";
    } else if (aFocus == gBrowser.selectedBrowser) {
      friendlyExpected = "Web document";
    }
  }
  info("Listening on item " + (expected.id || expected.className));
  let focused = BrowserTestUtils.waitForEvent(expected, "focus", aAncestorOk);
  EventUtils.synthesizeKey(key, {shiftKey: shift});
  let receivedEvent = await focused;
  info("Got focus on item: " + (receivedEvent.target.id || receivedEvent.target.className));
  ok(true, friendlyExpected + " focused after " + aKey + " pressed");
}

function startFromUrlBar() {
  gURLBar.focus();
  is(document.activeElement, gURLBar.inputField,
     "URL bar focused for start of test");
}

// The Reload button is disabled for a short time even after the page finishes
// loading. Wait for it to be enabled.
async function waitUntilReloadEnabled() {
  let button = document.getElementById("reload-button");
  await TestUtils.waitForCondition(() => !button.disabled);
}

// Opens a new, blank tab, executes a task and closes the tab.
function withNewBlankTab(taskFn) {
  return BrowserTestUtils.withNewTab("about:blank", async function() {
    // For a blank tab, the Reload button should be disabled. However, when we
    // open about:blank with BrowserTestUtils.withNewTab, this is unreliable.
    // Therefore, explicitly disable the reload command.
    // We disable the command (rather than disabling the button directly) so the
    // button will be updated correctly for future page loads.
    document.getElementById("Browser:Reload").setAttribute("disabled", "true");
    await taskFn();
  });
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.toolbars.keyboard_navigation", true],
      ["accessibility.tabfocus", 7],
    ],
  });
  resetToolbarWithoutDevEditionButtons();
});

// Test tab stops with no page loaded.
add_task(async function testTabStopsNoPage() {
  await withNewBlankTab(async function() {
    startFromUrlBar();
    await expectFocusAfterKey("Shift+Tab", "home-button");
    await expectFocusAfterKey("Shift+Tab", "tabbrowser-tabs", true);
    await expectFocusAfterKey("Tab", "home-button");
    await expectFocusAfterKey("Tab", gURLBar.inputField);
    await expectFocusAfterKey("Tab", "library-button");
    await expectFocusAfterKey("Tab", gBrowser.selectedBrowser);
  });
});

// Test tab stops with a page loaded.
add_task(async function testTabStopsPageLoaded() {
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    await waitUntilReloadEnabled();
    startFromUrlBar();
    await expectFocusAfterKey("Shift+Tab", "identity-box");
    await expectFocusAfterKey("Shift+Tab", "reload-button");
    await expectFocusAfterKey("Shift+Tab", "tabbrowser-tabs", true);
    await expectFocusAfterKey("Tab", "reload-button");
    await expectFocusAfterKey("Tab", "identity-box");
    await expectFocusAfterKey("Tab", gURLBar.inputField);
    await expectFocusAfterKey("Tab", "pageActionButton");
    await expectFocusAfterKey("Tab", "library-button");
    await expectFocusAfterKey("Tab", gBrowser.selectedBrowser);
  });
});

// Test tab stops with a notification anchor visible.
// The notification anchor should not get its own tab stop.
add_task(async function testTabStopsWithNotification() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(aBrowser) {
    let popupShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    // Request a permission.
    BrowserTestUtils.synthesizeMouseAtCenter("#geo", {}, aBrowser);
    await popupShown;
    startFromUrlBar();
    // If the notification anchor were in the tab order, the next shift+tab
    // would focus it instead of #identity-box.
    await expectFocusAfterKey("Shift+Tab", "identity-box");
  });
});

// Test tab stops with the Bookmarks toolbar visible.
add_task(async function testTabStopsWithBookmarksToolbar() {
  await BrowserTestUtils.withNewTab("about:blank", async function() {
    CustomizableUI.setToolbarVisibility("PersonalToolbar", true);
    startFromUrlBar();
    await expectFocusAfterKey("Tab", "library-button");
    await expectFocusAfterKey("Tab", "PersonalToolbar", true);
    await expectFocusAfterKey("Tab", gBrowser.selectedBrowser);

    // Make sure the Bookmarks toolbar is no longer tabbable once hidden.
    CustomizableUI.setToolbarVisibility("PersonalToolbar", false);
    startFromUrlBar();
    await expectFocusAfterKey("Tab", "library-button");
    await expectFocusAfterKey("Tab", gBrowser.selectedBrowser);
  });
});

// Test a focusable toolbartabstop which has no navigable buttons.
add_task(async function testTabStopNoButtons() {
  await withNewBlankTab(async function() {
    // The Back, Forward and Reload buttons are all currently disabled.
    // The Home button is the only other button at that tab stop.
    CustomizableUI.removeWidgetFromArea("home-button");
    startFromUrlBar();
    await expectFocusAfterKey("Shift+Tab", "tabbrowser-tabs", true);
    await expectFocusAfterKey("Tab", gURLBar.inputField);
    resetToolbarWithoutDevEditionButtons();
    // Make sure the button is reachable now that it has been re-added.
    await expectFocusAfterKey("Shift+Tab", "home-button", true);
  });
});

// Test that right/left arrows move through toolbarbuttons.
// This also verifies that:
// 1. Right/left arrows do nothing when at the edges; and
// 2. The overflow menu button can't be reached by right arrow when it isn't
// visible.
add_task(async function testArrowsToolbarbuttons() {
  await BrowserTestUtils.withNewTab("about:blank", async function() {
    startFromUrlBar();
    await expectFocusAfterKey("Tab", "library-button");
    EventUtils.synthesizeKey("KEY_ArrowLeft");
    is(document.activeElement.id, "library-button",
       "ArrowLeft at end of button group does nothing");
    await expectFocusAfterKey("ArrowRight", "sidebar-button");
    // This next check also confirms that the overflow menu button is skipped,
    // since it is currently invisible.
    await expectFocusAfterKey("ArrowRight", "PanelUI-menu-button");
    EventUtils.synthesizeKey("KEY_ArrowRight");
    is(document.activeElement.id, "PanelUI-menu-button",
       "ArrowRight at end of button group does nothing");
    await expectFocusAfterKey("ArrowLeft", "sidebar-button");
    await expectFocusAfterKey("ArrowLeft", "library-button");
  });
});

// Test that right/left arrows move through buttons wihch aren't toolbarbuttons
// but have role="button".
add_task(async function testArrowsRoleButton() {
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    startFromUrlBar();
    await expectFocusAfterKey("Tab", "pageActionButton");
    await expectFocusAfterKey("ArrowRight", "pocket-button");
    await expectFocusAfterKey("ArrowRight", "star-button");
    await expectFocusAfterKey("ArrowLeft", "pocket-button");
    await expectFocusAfterKey("ArrowLeft", "pageActionButton");
  });
});

// Test that right/left arrows do not land on disabled buttons.
add_task(async function testArrowsDisabledButtons() {
  await BrowserTestUtils.withNewTab("https://example.com", async function(aBrowser) {
    await waitUntilReloadEnabled();
    startFromUrlBar();
    await expectFocusAfterKey("Shift+Tab", "identity-box");
    // Back and Forward buttons are disabled.
    await expectFocusAfterKey("Shift+Tab", "reload-button");
    EventUtils.synthesizeKey("KEY_ArrowLeft");
    is(document.activeElement.id, "reload-button",
       "ArrowLeft on Reload button when prior buttons disabled does nothing");

    BrowserTestUtils.loadURI(aBrowser, "https://example.com/2");
    await BrowserTestUtils.browserLoaded(aBrowser);
    await waitUntilReloadEnabled();
    startFromUrlBar();
    await expectFocusAfterKey("Shift+Tab", "identity-box");
    await expectFocusAfterKey("Shift+Tab", "back-button");
    // Forward button is still disabled.
    await expectFocusAfterKey("ArrowRight", "reload-button");
  });
});

// Test that right arrow reaches the overflow menu button when it is visible.
add_task(async function testArrowsOverflowButton() {
  await BrowserTestUtils.withNewTab("about:blank", async function() {
    // Move something to the overflow menu to make the button appear.
    CustomizableUI.addWidgetToArea("home-button", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
    startFromUrlBar();
    await expectFocusAfterKey("Tab", "library-button");
    await expectFocusAfterKey("ArrowRight", "sidebar-button");
    await expectFocusAfterKey("ArrowRight", "nav-bar-overflow-button");
    await expectFocusAfterKey("ArrowRight", "PanelUI-menu-button");
    await expectFocusAfterKey("ArrowLeft", "nav-bar-overflow-button");
    // Make sure the button is not reachable once it is invisible again.
    await expectFocusAfterKey("ArrowRight", "PanelUI-menu-button");
    resetToolbarWithoutDevEditionButtons();
    // Flush layout so its invisibility can be detected.
    document.getElementById("nav-bar-overflow-button").clientWidth;
    await expectFocusAfterKey("ArrowLeft", "sidebar-button");
  });
});

// Test that toolbar keyboard navigation doesn't interfere with PanelMultiView
// keyboard navigation.
// We do this by opening the Library menu and ensuring that pressing left arrow
// does nothing.
add_task(async function testArrowsInPanelMultiView() {
  let button = document.getElementById("library-button");
  forceFocus(button);
  let view = document.getElementById("appMenu-libraryView");
  let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
  EventUtils.synthesizeKey(" ");
  let focusEvt = await focused;
  ok(true, "Focus inside Library menu after toolbar button pressed");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  is(document.activeElement, focusEvt.target,
     "ArrowLeft inside panel does nothing");
  let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
  view.closest("panel").hidePopup();
  await hidden;
});

registerCleanupFunction(() => CustomizableUI.reset());
