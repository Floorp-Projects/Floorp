/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(2);

const isOSX = (Services.appinfo.OS === "Darwin");

// Right-click on the home button should
// show a context menu with options to move it.
add_task(async function home_button_context() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  let contextMenu = document.getElementById("toolbar-context-menu");
  let shownPromise = popupShown(contextMenu);
  let homeButton = document.getElementById("home-button");
  EventUtils.synthesizeMouse(homeButton, 2, 2, {type: "contextmenu", button: 2 });
  await shownPromise;

  let expectedEntries = [
    [".customize-context-moveToPanel", true],
    [".customize-context-removeFromToolbar", true],
    ["---"]
  ];
  if (!isOSX) {
    expectedEntries.push(["#toggle_toolbar-menubar", true]);
  }
  expectedEntries.push(
    ["#toggle_PersonalToolbar", true],
    ["---"],
    [".viewCustomizeToolbar", true]
  );
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenPromise;
});

// Right-click on an empty bit of tabstrip should
// show a context menu without options to move it,
// but with tab-specific options instead.
add_task(async function tabstrip_context() {
  // ensure there are tabs to reload/bookmark:
  let extraTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  let contextMenu = document.getElementById("toolbar-context-menu");
  let shownPromise = popupShown(contextMenu);
  let tabstrip = document.getElementById("tabbrowser-tabs");
  let rect = tabstrip.getBoundingClientRect();
  EventUtils.synthesizeMouse(tabstrip, rect.width - 2, 2, {type: "contextmenu", button: 2 });
  await shownPromise;

  let closedTabsAvailable = SessionStore.getClosedTabCount(window) == 0;
  info("Closed tabs: " + closedTabsAvailable);
  let expectedEntries = [
    ["#toolbar-context-reloadAllTabs", true],
    ["#toolbar-context-bookmarkAllTabs", true],
    ["#toolbar-context-undoCloseTab", !closedTabsAvailable],
    ["---"]
  ];
  if (!isOSX) {
    expectedEntries.push(["#toggle_toolbar-menubar", true]);
  }
  expectedEntries.push(
    ["#toggle_PersonalToolbar", true],
    ["---"],
    [".viewCustomizeToolbar", true]
  );
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenPromise;
  await BrowserTestUtils.removeTab(extraTab);
});

// Right-click on an empty bit of extra toolbar should
// show a context menu with moving options disabled,
// and a toggle option for the extra toolbar
add_task(async function empty_toolbar_context() {
  let contextMenu = document.getElementById("toolbar-context-menu");
  let shownPromise = popupShown(contextMenu);
  let toolbar = createToolbarWithPlacements("880164_empty_toolbar", []);
  toolbar.setAttribute("context", "toolbar-context-menu");
  toolbar.setAttribute("toolbarname", "Fancy Toolbar for Context Menu");
  EventUtils.synthesizeMouseAtCenter(toolbar, {type: "contextmenu", button: 2 });
  await shownPromise;

  let expectedEntries = [
    [".customize-context-moveToPanel", false],
    [".customize-context-removeFromToolbar", false],
    ["---"]
  ];
  if (!isOSX) {
    expectedEntries.push(["#toggle_toolbar-menubar", true]);
  }
  expectedEntries.push(
    ["#toggle_PersonalToolbar", true],
    ["#toggle_880164_empty_toolbar", true],
    ["---"],
    [".viewCustomizeToolbar", true]
  );
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenPromise;
  removeCustomToolbars();
});


// Right-click on the urlbar-container should
// show a context menu with disabled options to move it.
add_task(async function urlbar_context() {
  let contextMenu = document.getElementById("toolbar-context-menu");
  let shownPromise = popupShown(contextMenu);
  let urlBarContainer = document.getElementById("urlbar-container");
  // Need to make sure not to click within an edit field.
  EventUtils.synthesizeMouse(urlBarContainer, 100, 1, {type: "contextmenu", button: 2 });
  await shownPromise;

  let expectedEntries = [
    [".customize-context-moveToPanel", false],
    [".customize-context-removeFromToolbar", false],
    ["---"]
  ];
  if (!isOSX) {
    expectedEntries.push(["#toggle_toolbar-menubar", true]);
  }
  expectedEntries.push(
    ["#toggle_PersonalToolbar", true],
    ["---"],
    [".viewCustomizeToolbar", true]
  );
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenPromise;
});

// Right-click on the searchbar and moving it to the menu
// and back should move the search-container instead.
add_task(async function searchbar_context_move_to_panel_and_back() {
  let searchbar = document.getElementById("searchbar");
  gCustomizeMode.addToPanel(searchbar);
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_PANEL, "Should be in panel");

  let shownPanelPromise = promisePanelShown(window);
  PanelUI.toggle({type: "command"});
  await shownPanelPromise;
  let hiddenPanelPromise = promisePanelHidden(window);
  PanelUI.toggle({type: "command"});
  await hiddenPanelPromise;

  gCustomizeMode.addToToolbar(searchbar);
  placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_NAVBAR, "Should be in navbar");
  gCustomizeMode.removeFromArea(searchbar);
  placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement, null, "Should be in palette");
  CustomizableUI.reset();
  placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_NAVBAR, "Should be in navbar");
});

// Right-click on an item within the menu panel should
// show a context menu with options to move it.
add_task(async function context_within_panel() {
  let shownPanelPromise = promisePanelShown(window);
  PanelUI.toggle({type: "command"});
  await shownPanelPromise;

  let contextMenu = document.getElementById("customizationPanelItemContextMenu");
  let shownContextPromise = popupShown(contextMenu);
  let newWindowButton = document.getElementById("new-window-button");
  ok(newWindowButton, "new-window-button was found");
  EventUtils.synthesizeMouse(newWindowButton, 2, 2, {type: "contextmenu", button: 2});
  await shownContextPromise;

  is(PanelUI.panel.state, "open", "The PanelUI should still be open.");

  let expectedEntries = [
    [".customize-context-moveToToolbar", true],
    [".customize-context-removeFromPanel", true],
    ["---"],
    [".viewCustomizeToolbar", true]
  ];
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenContextPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenContextPromise;

  let hiddenPromise = promisePanelHidden(window);
  PanelUI.toggle({type: "command"});
  await hiddenPromise;
});

// Right-click on the home button while in customization mode
// should show a context menu with options to move it.
add_task(async function context_home_button_in_customize_mode() {
  await startCustomizing();
  let contextMenu = document.getElementById("toolbar-context-menu");
  let shownPromise = popupShown(contextMenu);
  let homeButton = document.getElementById("wrapper-home-button");
  EventUtils.synthesizeMouse(homeButton, 2, 2, {type: "contextmenu", button: 2});
  await shownPromise;

  let expectedEntries = [
    [".customize-context-moveToPanel", true],
    [".customize-context-removeFromToolbar", true],
    ["---"]
  ];
  if (!isOSX) {
    expectedEntries.push(["#toggle_toolbar-menubar", true]);
  }
  expectedEntries.push(
    ["#toggle_PersonalToolbar", true],
    ["---"],
    [".viewCustomizeToolbar", false]
  );
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenContextPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenContextPromise;
});

// Right-click on an item in the palette should
// show a context menu with options to move it.
add_task(async function context_click_in_palette() {
  let contextMenu = document.getElementById("customizationPaletteItemContextMenu");
  let shownPromise = popupShown(contextMenu);
  let openFileButton = document.getElementById("wrapper-open-file-button");
  EventUtils.synthesizeMouse(openFileButton, 2, 2, {type: "contextmenu", button: 2});
  await shownPromise;

  let expectedEntries = [
    [".customize-context-addToToolbar", true],
    [".customize-context-addToPanel", true]
  ];
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenContextPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenContextPromise;
});

// Right-click on an item in the panel while in customization mode
// should show a context menu with options to move it.
add_task(async function context_click_in_customize_mode() {
  let contextMenu = document.getElementById("customizationPanelItemContextMenu");
  let shownPromise = popupShown(contextMenu);
  let newWindowButton = document.getElementById("wrapper-new-window-button");
  EventUtils.synthesizeMouse(newWindowButton, 2, 2, {type: "contextmenu", button: 2});
  await shownPromise;

  let expectedEntries = [
    [".customize-context-moveToToolbar", true],
    [".customize-context-removeFromPanel", true],
    ["---"],
    [".viewCustomizeToolbar", false]
  ];
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenContextPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenContextPromise;
  await endCustomizing();
});

// Test the toolbarbutton panel context menu in customization mode
// without opening the panel before customization mode
add_task(async function context_click_customize_mode_panel_not_opened() {
  this.otherWin = await openAndLoadWindow(null, true);

  await new Promise(resolve => waitForFocus(resolve, this.otherWin));

  await startCustomizing(this.otherWin);

  let contextMenu = this.otherWin.document.getElementById("customizationPanelItemContextMenu");
  let shownPromise = popupShown(contextMenu);
  let newWindowButton = this.otherWin.document.getElementById("wrapper-new-window-button");
  EventUtils.synthesizeMouse(newWindowButton, 2, 2, {type: "contextmenu", button: 2}, this.otherWin);
  await shownPromise;

  let expectedEntries = [
    [".customize-context-moveToToolbar", true],
    [".customize-context-removeFromPanel", true],
    ["---"],
    [".viewCustomizeToolbar", false]
  ];
  checkContextMenu(contextMenu, expectedEntries, this.otherWin);

  let hiddenContextPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenContextPromise;
  await endCustomizing(this.otherWin);
  await promiseWindowClosed(this.otherWin);
  this.otherWin = null;

  await new Promise(resolve => waitForFocus(resolve, window));
});

// Bug 945191 - Combined buttons show wrong context menu options
// when they are in the toolbar.
add_task(async function context_combined_buttons_toolbar() {
  await startCustomizing();
  let contextMenu = document.getElementById("customizationPanelItemContextMenu");
  let shownPromise = popupShown(contextMenu);
  let zoomControls = document.getElementById("wrapper-zoom-controls");
  EventUtils.synthesizeMouse(zoomControls, 2, 2, {type: "contextmenu", button: 2});
  await shownPromise;
  // Execute the command to move the item from the panel to the toolbar.
  let moveToToolbar = contextMenu.querySelector(".customize-context-moveToToolbar");
  moveToToolbar.doCommand();
  let hiddenPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenPromise;
  await endCustomizing();

  zoomControls = document.getElementById("zoom-controls");
  is(zoomControls.parentNode.id, "nav-bar-customization-target", "Zoom-controls should be on the nav-bar");

  contextMenu = document.getElementById("toolbar-context-menu");
  shownPromise = popupShown(contextMenu);
  EventUtils.synthesizeMouse(zoomControls, 2, 2, {type: "contextmenu", button: 2});
  await shownPromise;

  let expectedEntries = [
    [".customize-context-moveToPanel", true],
    [".customize-context-removeFromToolbar", true],
    ["---"]
  ];
  if (!isOSX) {
    expectedEntries.push(["#toggle_toolbar-menubar", true]);
  }
  expectedEntries.push(
    ["#toggle_PersonalToolbar", true],
    ["---"],
    [".viewCustomizeToolbar", true]
  );
  checkContextMenu(contextMenu, expectedEntries);

  hiddenPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenPromise;
  await resetCustomization();
});

// Bug 947586 - After customization, panel items show wrong context menu options
add_task(async function context_after_customization_panel() {
  await startCustomizing();
  await endCustomizing();

  await PanelUI.show();

  let contextMenu = document.getElementById("customizationPanelItemContextMenu");
  let shownContextPromise = popupShown(contextMenu);
  let newWindowButton = document.getElementById("new-window-button");
  ok(newWindowButton, "new-window-button was found");
  EventUtils.synthesizeMouse(newWindowButton, 2, 2, {type: "contextmenu", button: 2});
  await shownContextPromise;

  is(PanelUI.panel.state, "open", "The PanelUI should still be open.");

  let expectedEntries = [
    [".customize-context-moveToToolbar", true],
    [".customize-context-removeFromPanel", true],
    ["---"],
    [".viewCustomizeToolbar", true]
  ];
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenContextPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenContextPromise;

  let hiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  await hiddenPromise;
});


// Bug 982027 - moving icon around removes custom context menu.
add_task(async function custom_context_menus() {
  let widgetId = "custom-context-menu-toolbarbutton";
  let expectedContext = "myfancycontext";
  let widget = createDummyXULButton(widgetId, "Test ctxt menu");
  widget.setAttribute("context", expectedContext);
  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_NAVBAR);
  is(widget.getAttribute("context"), expectedContext, "Should have context menu when added to the toolbar.");

  await startCustomizing();
  is(widget.getAttribute("context"), "", "Should not have own context menu in the toolbar now that we're customizing.");
  is(widget.getAttribute("wrapped-context"), expectedContext, "Should keep own context menu wrapped when in toolbar.");

  let panel = PanelUI.contents;
  simulateItemDrag(widget, panel);
  is(widget.getAttribute("context"), "", "Should not have own context menu when in the panel.");
  is(widget.getAttribute("wrapped-context"), expectedContext, "Should keep own context menu wrapped now that we're in the panel.");

  simulateItemDrag(widget, document.getElementById("nav-bar").customizationTarget);
  is(widget.getAttribute("context"), "", "Should not have own context menu when back in toolbar because we're still customizing.");
  is(widget.getAttribute("wrapped-context"), expectedContext, "Should keep own context menu wrapped now that we're back in the toolbar.");

  await endCustomizing();
  is(widget.getAttribute("context"), expectedContext, "Should have context menu again now that we're out of customize mode.");
  CustomizableUI.removeWidgetFromArea(widgetId);
  widget.remove();
  ok(CustomizableUI.inDefaultState, "Should be in default state after removing button.");
});
