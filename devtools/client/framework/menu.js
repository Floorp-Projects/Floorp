/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const EventEmitter = require("devtools/shared/event-emitter");
const { getCurrentZoom } = require("devtools/shared/layout/utils");

/**
 * A partial implementation of the Menu API provided by electron:
 * https://github.com/electron/electron/blob/master/docs/api/menu.md.
 *
 * Extra features:
 *  - Emits an 'open' and 'close' event when the menu is opened/closed

 * @param String id (non standard)
 *        Needed so tests can confirm the XUL implementation is working
 */
function Menu({ id = null } = {}) {
  this.menuitems = [];
  this.id = id;

  Object.defineProperty(this, "items", {
    get() {
      return this.menuitems;
    }
  });

  EventEmitter.decorate(this);
}

/**
 * Add an item to the end of the Menu
 *
 * @param {MenuItem} menuItem
 */
Menu.prototype.append = function(menuItem) {
  this.menuitems.push(menuItem);
};

/**
 * Add an item to a specified position in the menu
 *
 * @param {int} pos
 * @param {MenuItem} menuItem
 */
Menu.prototype.insert = function(pos, menuItem) {
  throw Error("Not implemented");
};

/**
 * Show the Menu with anchor element's coordinate.
 * For example, In the case of zoom in/out the devtool panel, we should multiply
 * element's position to zoom value.
 * If you know the screen coodinate of display position, you should use Menu.pop().
 *
 * @param {int} x
 * @param {int} y
 * @param Toolbox toolbox
 */
Menu.prototype.popupWithZoom = function(x, y, toolbox) {
  const zoom = getCurrentZoom(toolbox.doc);
  this.popup(x * zoom, y * zoom, toolbox);
};

/**
 * Show the Menu at a specified location on the screen
 *
 * Missing features:
 *   - browserWindow - BrowserWindow (optional) - Default is null.
 *   - positioningItem Number - (optional) OS X
 *
 * @param {int} screenX
 * @param {int} screenY
 * @param Toolbox toolbox (non standard)
 *        Needed so we in which window to inject XUL
 */
Menu.prototype.popup = function(screenX, screenY, toolbox) {
  const doc = toolbox.doc;

  let popupset = doc.querySelector("popupset");
  if (!popupset) {
    popupset = doc.createElementNS(XUL_NS, "popupset");
    doc.documentElement.appendChild(popupset);
  }
  // See bug 1285229, on Windows, opening the same popup multiple times in a
  // row ends up duplicating the popup. The newly inserted popup doesn't
  // dismiss the old one. So remove any previously displayed popup before
  // opening a new one.
  let popup = popupset.querySelector("menupopup[menu-api=\"true\"]");
  if (popup) {
    popup.hidePopup();
  }

  popup = doc.createElementNS(XUL_NS, "menupopup");
  popup.setAttribute("menu-api", "true");
  popup.setAttribute("consumeoutsideclicks", "true");

  if (this.id) {
    popup.id = this.id;
  }
  this._createMenuItems(popup);

  // Remove the menu from the DOM once it's hidden.
  popup.addEventListener("popuphidden", (e) => {
    if (e.target === popup) {
      popup.remove();
      this.emit("close");
    }
  });

  popup.addEventListener("popupshown", (e) => {
    if (e.target === popup) {
      this.emit("open");
    }
  });

  popupset.appendChild(popup);
  popup.openPopupAtScreen(screenX, screenY, true);
};

Menu.prototype._createMenuItems = function(parent) {
  const doc = parent.ownerDocument;
  this.menuitems.forEach(item => {
    if (!item.visible) {
      return;
    }

    if (item.submenu) {
      const menupopup = doc.createElementNS(XUL_NS, "menupopup");
      item.submenu._createMenuItems(menupopup);

      const menu = doc.createElementNS(XUL_NS, "menu");
      menu.appendChild(menupopup);
      applyItemAttributesToNode(item, menu);
      parent.appendChild(menu);
    } else if (item.type === "separator") {
      const menusep = doc.createElementNS(XUL_NS, "menuseparator");
      parent.appendChild(menusep);
    } else {
      const menuitem = doc.createElementNS(XUL_NS, "menuitem");
      applyItemAttributesToNode(item, menuitem);

      menuitem.addEventListener("command", () => {
        item.click();
      });
      menuitem.addEventListener("DOMMenuItemActive", () => {
        item.hover();
      });

      parent.appendChild(menuitem);
    }
  });
};

Menu.setApplicationMenu = () => {
  throw Error("Not implemented");
};

Menu.sendActionToFirstResponder = () => {
  throw Error("Not implemented");
};

Menu.buildFromTemplate = () => {
  throw Error("Not implemented");
};

function applyItemAttributesToNode(item, node) {
  if (item.l10nID) {
    node.setAttribute("data-l10n-id", item.l10nID);
  } else {
    node.setAttribute("label", item.label);
    if (item.accelerator) {
      node.setAttribute("acceltext", item.accelerator);
    }
    if (item.accesskey) {
      node.setAttribute("accesskey", item.accesskey);
    }
  }
  if (item.type === "checkbox") {
    node.setAttribute("type", "checkbox");
  }
  if (item.type === "radio") {
    node.setAttribute("type", "radio");
  }
  if (item.disabled) {
    node.setAttribute("disabled", "true");
  }
  if (item.checked) {
    node.setAttribute("checked", "true");
  }
  if (item.id) {
    node.id = item.id;
  }
}

module.exports = Menu;
