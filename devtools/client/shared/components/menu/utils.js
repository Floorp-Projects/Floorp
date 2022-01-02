/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

/**
 * Helper function for opening context menu.
 *
 * @param {Array} items
 *        List of menu items.
 * @param {Object} options:
 * @property {Element} button
 *           Button element used to open the menu.
 * @property {Number} screenX
 *           Screen x coordinate of the menu on the screen.
 * @property {Number} screenY
 *           Screen y coordinate of the menu on the screen.
 */
function showMenu(items, options) {
  if (items.length === 0) {
    return;
  }

  // Build the menu object from provided menu items.
  const menu = new Menu();
  items.forEach(item => {
    if (item == "-") {
      item = { type: "separator" };
    }

    const menuItem = new MenuItem(item);
    const subItems = item.submenu;

    if (subItems) {
      const subMenu = new Menu();
      subItems.forEach(subItem => {
        subMenu.append(new MenuItem(subItem));
      });
      menuItem.submenu = subMenu;
    }

    menu.append(menuItem);
  });

  // Calculate position on the screen according to
  // the parent button if available.
  if (options.button) {
    menu.popupAtTarget(options.button, window.document);
  } else {
    const screenX = options.screenX;
    const screenY = options.screenY;
    menu.popup(screenX, screenY, window.document);
  }
}

module.exports = {
  showMenu,
};
