/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const Menu = require("resource://devtools/client/framework/menu.js");
const MenuItem = require("resource://devtools/client/framework/menu-item.js");

export function showMenu(evt, items) {
  if (items.length === 0) {
    return;
  }

  const menu = new Menu();
  items
    .filter(item => item.visible === undefined || item.visible === true)
    .forEach(item => {
      const menuItem = new MenuItem(item);
      menuItem.submenu = createSubMenu(item.submenu);
      menu.append(menuItem);
    });

  menu.popup(evt.screenX, evt.screenY, window.parent.document);
}

function createSubMenu(subItems) {
  if (subItems) {
    const subMenu = new Menu();
    subItems.forEach(subItem => {
      subMenu.append(new MenuItem(subItem));
    });
    return subMenu;
  }
  return null;
}

export function buildMenu(items) {
  return items
    .map(itm => {
      const hide = typeof itm.hidden === "function" ? itm.hidden() : itm.hidden;
      return hide ? null : itm.item;
    })
    .filter(itm => itm !== null);
}
