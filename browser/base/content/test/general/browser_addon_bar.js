/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let addonbar = document.getElementById("addon-bar");
  ok(addonbar.collapsed, "addon bar is collapsed by default");

  let topMenu, toolbarMenu;

  function onTopMenuShown(event) {
    ok(1, "top menu popupshown listener called");
    event.currentTarget.removeEventListener("popupshown", arguments.callee, false);
    // open the customize or toolbars menu
    toolbarMenu = document.getElementById("appmenu_customizeMenu") ||
                      document.getElementById("viewToolbarsMenu").firstElementChild;
    toolbarMenu.addEventListener("popupshown", onToolbarMenuShown, false);
    toolbarMenu.addEventListener("popuphidden", onToolbarMenuHidden, false);
    toolbarMenu.openPopup();
  }

  function onTopMenuHidden(event) {
    ok(1, "top menu popuphidden listener called");
    event.currentTarget.removeEventListener("popuphidden", arguments.callee, false);
    finish();
  }

  function onToolbarMenuShown(event) {
    ok(1, "sub menu popupshown listener called");
    event.currentTarget.removeEventListener("popupshown", arguments.callee, false);

    // test the menu item's default state
    let menuitem = document.getElementById("toggle_addon-bar");
    ok(menuitem, "found the menu item");
    is(menuitem.getAttribute("checked"), "false", "menuitem is not checked by default");

    // click on the menu item
    // TODO: there's got to be a way to check+command in one shot
    menuitem.setAttribute("checked", "true");
    menuitem.click();

    // now the addon bar should be visible and the menu checked
    is(addonbar.getAttribute("collapsed"), "false", "addon bar is visible after executing the command");
    is(menuitem.getAttribute("checked"), "true", "menuitem is checked after executing the command");

    toolbarMenu.hidePopup();
  }

  function onToolbarMenuHidden(event) {
    ok(1, "toolbar menu popuphidden listener called");
    event.currentTarget.removeEventListener("popuphidden", arguments.callee, false);
    topMenu.hidePopup();
  }

  // open the appmenu or view menu
  topMenu = document.getElementById("appmenu-popup") ||
            document.getElementById("menu_viewPopup");
  topMenu.addEventListener("popupshown", onTopMenuShown, false);
  topMenu.addEventListener("popuphidden", onTopMenuHidden, false);
  topMenu.openPopup();
}
