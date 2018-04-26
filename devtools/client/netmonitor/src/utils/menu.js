/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

function showMenu(evt, items) {
  if (items.length === 0) {
    return;
  }

  let menu = new Menu();
  items.forEach((item) => {
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

  menu.popup(evt.screenX, evt.screenY, { doc: window.parent.document });
}

module.exports = {
  showMenu,
};
