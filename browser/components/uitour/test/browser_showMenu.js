"use strict";

const CONTROL_CENTER_PANEL = gIdentityHandler._identityPopup;
const CONTROL_CENTER_MENU_NAME = "controlCenter";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(async function test_showMenu_controlCenter() {
  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should initially be hidden");
  await showMenuPromise(CONTROL_CENTER_MENU_NAME);
  is_element_visible(CONTROL_CENTER_PANEL, "Panel should be visible after showMenu");

  await gURLBar.focus();
  is_element_visible(CONTROL_CENTER_PANEL, "Panel should remain visible after focus outside");

  await showMenuPromise(CONTROL_CENTER_MENU_NAME);
  is_element_visible(CONTROL_CENTER_PANEL,
                     "Panel should remain visible and callback called after a 2nd showMenu");

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, function() {
    ok(true, "Tab opened");
  });

  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should hide upon tab switch");
});

add_UITour_task(async function test_hideMenu_controlCenter() {
  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should initially be hidden");
  await showMenuPromise(CONTROL_CENTER_MENU_NAME);
  is_element_visible(CONTROL_CENTER_PANEL, "Panel should be visible after showMenu");
  let hidePromise = promisePanelElementHidden(window, CONTROL_CENTER_PANEL);
  await gContentAPI.hideMenu(CONTROL_CENTER_MENU_NAME);
  await hidePromise;

  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should hide after hideMenu");
});

add_UITour_task(async function test_showMenu_hideMenu_urlbarPopup() {
  let shownPromise = promisePanelElementShown(window, gURLBar.popup);
  await showMenuPromise("urlbar");
  await shownPromise;
  is(gURLBar.popup.state, "open", "The urlbar popup should open after showMenu");
  is(gURLBar.controller.searchString, "Firefox", "Search string is Firefox");
  let hidePromise = promisePanelElementHidden(window, gURLBar.popup);
  await gContentAPI.hideMenu("urlbar");
  await hidePromise;
  is(gURLBar.popup.state, "closed", "The urlbar popup should close after hideMenu");
});

add_UITour_task(async function test_showMenu_hideMenu_pageActionPanel() {
  let pageActionPanel = BrowserPageActions.panelNode;
  let shownPromise = promisePanelElementShown(window, pageActionPanel);
  await showMenuPromise("pageActionPanel");
  await shownPromise;
  is(pageActionPanel.state, "open", "The page action panel should open after showMenu");
  let hidePromise = promisePanelElementHidden(window, pageActionPanel);
  await gContentAPI.hideMenu("pageActionPanel");
  await hidePromise;
  is(pageActionPanel.state, "closed", "The page action panel should close after hideMenu");
});
