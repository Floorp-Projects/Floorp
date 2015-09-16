/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the 'Log Request and Response Bodies' buttons can be toggled
// with keyboard.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 915141: Toggle log response bodies with keyboard";
var hud;

function test() {
  let saveBodiesMenuItem;
  let saveBodiesContextMenuItem;

  loadTab(TEST_URI).then(({tab: tab}) => {
    return openConsole(tab);
  })
  .then((aHud) => {
    hud = aHud;
    saveBodiesMenuItem = hud.ui.rootElement.querySelector("#saveBodies");
    saveBodiesContextMenuItem = hud.ui.rootElement.querySelector("#saveBodiesContextMenu");

    // Test the context menu action.
    info("Testing 'Log Request and Response Bodies' menuitem of right click " +
         "context menu.");

    return openPopup(saveBodiesContextMenuItem);
  })
  .then(() => {
    is(saveBodiesContextMenuItem.getAttribute("checked"), "false",
       "Context menu: 'log responses' is not checked before action.");
    is(hud.ui._saveRequestAndResponseBodies, false,
       "Context menu: Responses are not logged before action.");

    EventUtils.synthesizeKey("VK_DOWN", {});
    EventUtils.synthesizeKey("VK_RETURN", {});

    return waitForUpdate(saveBodiesContextMenuItem);
  })
  .then(() => {
    is(saveBodiesContextMenuItem.getAttribute("checked"), "true",
       "Context menu: 'log responses' is checked after menuitem was selected " +
       "with keyboard.");
    is(hud.ui._saveRequestAndResponseBodies, true,
       "Context menu: Responses are saved after menuitem was selected with " +
       "keyboard.");

    return openPopup(saveBodiesMenuItem);
  })
  .then(() => {
    // Test the 'Net' menu item.
    info("Testing 'Log Request and Response Bodies' menuitem of 'Net' menu " +
         "in the console.");
    // 'Log Request and Response Bodies' should be selected due to previous
    // test.

    is(saveBodiesMenuItem.getAttribute("checked"), "true",
       "Console net menu: 'log responses' is checked before action.");
    is(hud.ui._saveRequestAndResponseBodies, true,
       "Console net menu: Responses are logged before action.");

    // The correct item is the last one in the menu.
    EventUtils.synthesizeKey("VK_UP", {});
    EventUtils.synthesizeKey("VK_RETURN", {});

    return waitForUpdate(saveBodiesMenuItem);
  })
  .then(() => {
    is(saveBodiesMenuItem.getAttribute("checked"), "false",
       "Console net menu: 'log responses' is NOT checked after menuitem was " +
       "selected with keyboard.");
    is(hud.ui._saveRequestAndResponseBodies, false,
       "Responses are NOT saved after menuitem was selected with keyboard.");
    hud = null;
  })
  .then(finishTest);
}

/**
 * Opens and waits for the menu containing menuItem to open.
 * @param menuItem MenuItem
 *        A MenuItem in a menu that should be opened.
 * @return A promise that's resolved once menu is open.
 */
function openPopup(menuItem) {
  let menu = menuItem.parentNode;

  let menuOpened = promise.defer();
  let uiUpdated = promise.defer();
  // The checkbox menuitem is updated asynchronously on 'popupshowing' event so
  // it's better to wait for both the update to happen and the menu to open
  // before continuing or the test might fail due to a race between menu being
  // shown and the item updated to have the correct state.
  hud.ui.once("save-bodies-ui-toggled", uiUpdated.resolve);
  menu.addEventListener("popupshown", function onPopup() {
    menu.removeEventListener("popupshown", onPopup);
    menuOpened.resolve();
  });

  menu.openPopup();
  return Promise.all([menuOpened.promise, uiUpdated.promise]);
}

/**
 * Waits for the settings and menu containing menuItem to update.
 * @param menuItem MenuItem
 *        The menuitem that should be updated.
 * @return A promise that's resolved once the settings and menus are updated.
 */
function waitForUpdate(menuItem) {
  info("Waiting for settings update to complete.");
  let deferred = promise.defer();
  hud.ui.once("save-bodies-pref-reversed", function() {
    hud.ui.once("save-bodies-ui-toggled", deferred.resolve);
    // The checked state is only updated once the popup is shown.
    menuItem.parentNode.openPopup();
  });
  return deferred.promise;
}
