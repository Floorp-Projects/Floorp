/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is browser add-on bar test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Dietrich Ayala <dietrich@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
