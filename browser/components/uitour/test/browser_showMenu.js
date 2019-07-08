"use strict";

const CONTROL_CENTER_PANEL = gIdentityHandler._identityPopup;
const CONTROL_CENTER_MENU_NAME = "controlCenter";

const { UrlbarTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlbarTestUtils.jsm"
);

var gTestTab;
var gContentAPI;
var gContentWindow;

add_task(setup_UITourTest);

add_UITour_task(async function test_showMenu_controlCenter() {
  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should initially be hidden");
  await showMenuPromise(CONTROL_CENTER_MENU_NAME);
  is_element_visible(
    CONTROL_CENTER_PANEL,
    "Panel should be visible after showMenu"
  );

  await gURLBar.focus();
  is_element_visible(
    CONTROL_CENTER_PANEL,
    "Panel should remain visible after focus outside"
  );

  await showMenuPromise(CONTROL_CENTER_MENU_NAME);
  is_element_visible(
    CONTROL_CENTER_PANEL,
    "Panel should remain visible and callback called after a 2nd showMenu"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    function() {
      ok(true, "Tab opened");
    }
  );

  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should hide upon tab switch");
});

add_UITour_task(async function test_hideMenu_controlCenter() {
  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should initially be hidden");
  await showMenuPromise(CONTROL_CENTER_MENU_NAME);
  is_element_visible(
    CONTROL_CENTER_PANEL,
    "Panel should be visible after showMenu"
  );
  let hidePromise = promisePanelElementHidden(window, CONTROL_CENTER_PANEL);
  await gContentAPI.hideMenu(CONTROL_CENTER_MENU_NAME);
  await hidePromise;

  is_element_hidden(CONTROL_CENTER_PANEL, "Panel should hide after hideMenu");
});

add_UITour_task(async function test_showMenu_hideMenu_urlbarPopup() {
  await UrlbarTestUtils.promisePopupOpen(window, () =>
    showMenuPromise("urlbar")
  );
  is(gURLBar.value, "Firefox", "Search string is Firefox");
  await UrlbarTestUtils.promisePopupClose(window, () =>
    gContentAPI.hideMenu("urlbar")
  );
});

add_UITour_task(async function test_showMenu_hideMenu_pageActionPanel() {
  let pageActionPanel = BrowserPageActions.panelNode;
  let shownPromise = promisePanelElementShown(window, pageActionPanel);
  await showMenuPromise("pageActionPanel");
  await shownPromise;
  is(
    pageActionPanel.state,
    "open",
    "The page action panel should open after showMenu"
  );
  let hidePromise = promisePanelElementHidden(window, pageActionPanel);
  await gContentAPI.hideMenu("pageActionPanel");
  await hidePromise;
  is(
    pageActionPanel.state,
    "closed",
    "The page action panel should close after hideMenu"
  );
});
