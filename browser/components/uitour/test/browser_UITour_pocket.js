/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;
let button;

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  UITourTest();
}

let tests = [
  taskify(function* test_menu_show_navbar() {
    ise(button.open, false, "Menu should initially be closed");
    gContentAPI.showMenu("pocket");

    // The panel gets created dynamically.
    let widgetPanel = null;
    yield waitForConditionPromise(() => {
      widgetPanel = document.getElementById("customizationui-widget-panel");
      return widgetPanel && widgetPanel.state == "open";
    }, "Menu should be visible after showMenu()");

    ok(button.open, "Button should know its view is open");
    ok(!widgetPanel.hasAttribute("noautohide"), "@noautohide shouldn't be on the pocket panel");
    ok(button.hasAttribute("open"), "Pocket button should know that the menu is open");

    widgetPanel.hidePopup();
    checkPanelIsHidden(widgetPanel);
  }),
  taskify(function* test_menu_show_appMenu() {
    CustomizableUI.addWidgetToArea("pocket-button", CustomizableUI.AREA_PANEL);

    ise(PanelUI.multiView.hasAttribute("panelopen"), false, "Multiview should initially be closed");
    gContentAPI.showMenu("pocket");

    yield waitForConditionPromise(() => {
      return PanelUI.panel.state == "open";
    }, "Menu should be visible after showMenu()");

    ok(!PanelUI.panel.hasAttribute("noautohide"), "@noautohide shouldn't be on the pocket panel");
    ok(PanelUI.multiView.showingSubView, "Subview should be open");
    ok(PanelUI.multiView.hasAttribute("panelopen"), "Multiview should know it's open");

    PanelUI.showMainView();
    PanelUI.panel.hidePopup();
    checkPanelIsHidden(PanelUI.panel);
  }),
];

// End tests

function checkPanelIsHidden(aPanel) {
  if (aPanel.parentElement) {
    is_hidden(aPanel);
  } else {
    ok(!aPanel.parentElement, "Widget panel should have been removed");
  }
  is(button.hasAttribute("open"), false, "Pocket button should know that the panel is closed");
}

if (Services.prefs.getBoolPref("browser.pocket.enabled")) {
  let placement = CustomizableUI.getPlacementOfWidget("pocket-button");

  // Add the button to the nav-bar by default.
  if (!placement || placement.area != CustomizableUI.AREA_NAVBAR) {
    CustomizableUI.addWidgetToArea("pocket-button", CustomizableUI.AREA_NAVBAR);
  }
  registerCleanupFunction(() => {
    CustomizableUI.reset();
  });

  let widgetGroupWrapper = CustomizableUI.getWidget("pocket-button");
  button = widgetGroupWrapper.forWindow(window).node;
  ok(button, "Got button node");
} else {
  todo(false, "Pocket is disabled so skip its UITour tests");
  tests = [];
}
