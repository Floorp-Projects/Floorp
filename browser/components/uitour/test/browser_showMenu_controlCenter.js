"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const CONTROL_CENTER_PANEL = gIdentityHandler._identityPopup;
const CONTROL_CENTER_MENU_NAME = "controlCenter";

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(async function test_showMenu() {
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
    url: "about:blank"
  }, function() {
    ok(true, "Tab opened");
  });

  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should hide upon tab switch");
});

add_UITour_task(async function test_hideMenu() {
  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should initially be hidden");
  await showMenuPromise(CONTROL_CENTER_MENU_NAME);
  is_element_visible(CONTROL_CENTER_PANEL, "Panel should be visible after showMenu");
  let hidePromise = promisePanelElementHidden(window, CONTROL_CENTER_PANEL);
  await gContentAPI.hideMenu(CONTROL_CENTER_MENU_NAME);
  await hidePromise;

  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should hide after hideMenu");
});
