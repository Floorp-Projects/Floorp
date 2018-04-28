/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

/**
 * Helper function for opening context menu.
 *
 * @param {Array} items List of menu items.
 * @param {Object} options:
 * @property {Number} screenX coordinate of the menu on the screen
 * @property {Number} screenY coordinate of the menu on the screen
 * @property {Object} button parent used to open the menu
 */
function showMenu(items, options) {
  if (items.length === 0) {
    return;
  }

  // Build the menu object from provided menu items.
  let menu = new Menu();
  items.forEach((item) => {
    if (item == "-") {
      item = { type: "separator" };
    }

    let menuItem = new MenuItem(item);
    let subItems = item.submenu;

    if (subItems) {
      let subMenu = new Menu();
      subItems.forEach((subItem) => {
        subMenu.append(new MenuItem(subItem));
      });
      menuItem.submenu = subMenu;
    }

    menu.append(menuItem);
  });

  let screenX = options.screenX;
  let screenY = options.screenY;

  // Calculate position on the screen according to
  // the parent button if available.
  if (options.button) {
    const button = options.button;
    const rect = button.getBoundingClientRect();
    const defaultView = button.ownerDocument.defaultView;
    screenX = rect.left + defaultView.mozInnerScreenX;
    screenY = rect.bottom + defaultView.mozInnerScreenY;
  }

  menu.popup(screenX, screenY, { doc: window.parent.document });
}

module.exports = {
  showMenu,
};
