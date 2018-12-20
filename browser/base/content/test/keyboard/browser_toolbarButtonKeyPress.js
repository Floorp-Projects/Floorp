/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the behavior of key presses on various toolbar buttons.
 */

// Toolbar buttons aren't focusable because if they were, clicking them would
// focus them, which is undesirable. Therefore, they're only made focusable
// when a user is navigating with the keyboard. This function forces focus as
// is done during keyboard navigation.
function forceFocus(aElem) {
  aElem.setAttribute("tabindex", "-1");
  aElem.focus();
  aElem.removeAttribute("tabindex");
}

// Test activation of the app menu button from the keyboard.
// The app menu should appear and focus should move inside it.
add_task(async function testAppMenuButtonPress() {
  let button = document.getElementById("PanelUI-menu-button");
  forceFocus(button);
  let focused = BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "focus", true);
  EventUtils.synthesizeKey(" ");
  await focused;
  ok(true, "Focus inside app menu after toolbar button pressed");
  let hidden = BrowserTestUtils.waitForEvent(window.PanelUI.panel, "popuphidden");
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
  let button = document.getElementById("library-button");
  forceFocus(button);
  let view = document.getElementById("appMenu-libraryView");
  let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
  EventUtils.synthesizeKey(" ");
  await focused;
  ok(true, "Focus inside Library menu after toolbar button pressed");
  let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
  view.closest("panel").hidePopup();
  await hidden;
});

// Test activation of the Developer button from the keyboard.
// This is a customizable widget of type "view".
// The Developer menu should appear and focus should move inside it.
add_task(async function testDeveloperButtonPress() {
  CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_NAVBAR);
  let button = document.getElementById("developer-button");
  forceFocus(button);
  let view = document.getElementById("PanelUI-developer");
  let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
  EventUtils.synthesizeKey(" ");
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
  CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_NAVBAR);
  let button = document.getElementById("developer-button");
  forceFocus(button);
  EventUtils.synthesizeKey("KEY_Tab");
  await TestUtils.waitForTick();
  let panel = document.getElementById("PanelUI-developer").closest("panel");
  ok(!panel || panel.state == "closed", "Developer menu not open after tab");
  CustomizableUI.reset();
});

// Test activation of the Page actions button from the keyboard.
// The Page Actions menu should appear and focus should move inside it.
add_task(async function testPageActionsButtonPress() {
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    let button = document.getElementById("pageActionButton");
    forceFocus(button);
    let view = document.getElementById("pageActionPanelMainView");
    let focused = BrowserTestUtils.waitForEvent(view, "focus", true);
    EventUtils.synthesizeKey(" ");
    await focused;
    ok(true, "Focus inside Page Actions menu after toolbar button pressed");
    let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
    view.closest("panel").hidePopup();
    await hidden;
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
    let view = document.getElementById("pageAction-urlbar-sendToDevice-subview");
    ok(view.contains(document.activeElement),
       "Focus inside Page Actions menu after toolbar button pressed");
    let hidden = BrowserTestUtils.waitForEvent(document, "popuphidden", true);
    view.closest("panel").hidePopup();
    await hidden;
    PageActions.actionForID("sendToDevice").pinnedToUrlbar = false;
  });
});
