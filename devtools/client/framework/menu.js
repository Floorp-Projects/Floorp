/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");
const EventEmitter = require("resource://devtools/shared/event-emitter.js");

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
    },
  });

  EventEmitter.decorate(this);
}

/**
 * Add an item to the end of the Menu
 *
 * @param {MenuItem} menuItem
 */
Menu.prototype.append = function (menuItem) {
  this.menuitems.push(menuItem);
};

/**
 * Remove all items from the Menu
 */
Menu.prototype.clear = function () {
  this.menuitems = [];
};

/**
 * Add an item to a specified position in the menu
 *
 * @param {int} pos
 * @param {MenuItem} menuItem
 */
Menu.prototype.insert = function (pos, menuItem) {
  throw Error("Not implemented");
};

/**
 * Show the Menu next to the provided target. Anchor point is bottom-left.
 *
 * @param {Element} target
 *        The element to use as anchor.
 */
Menu.prototype.popupAtTarget = function (target) {
  const rect = target.getBoundingClientRect();
  const doc = target.ownerDocument;
  const defaultView = doc.defaultView;
  const x = rect.left + defaultView.mozInnerScreenX;
  const y = rect.bottom + defaultView.mozInnerScreenY;

  this.popup(x, y, doc);
};

/**
 * Hide an existing menu, if there's any.
 *
 * @param {Document} doc
 *        The document that should own the context menu.
 */
Menu.prototype.hide = function (doc) {
  const win = doc.defaultView;
  doc = DevToolsUtils.getTopWindow(win).document;
  const popup = doc.querySelector('popupset menupopup[menu-api="true"]');
  if (!popup) {
    return;
  }
  popup.hidePopup();
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
 * @param {Document} doc
 *        The document that should own the context menu.
 */
Menu.prototype.popup = function (screenX, screenY, doc) {
  // See bug 1285229, on Windows, opening the same popup multiple times in a
  // row ends up duplicating the popup. The newly inserted popup doesn't
  // dismiss the old one. So remove any previously displayed popup before
  // opening a new one.
  this.hide(doc);

  // The context-menu will be created in the topmost window to preserve keyboard
  // navigation (see Bug 1543940).
  // Keep a reference on the window owning the menu to hide the popup on unload.
  const win = doc.defaultView;
  const topWin = DevToolsUtils.getTopWindow(win);

  // Convert coordinates from win's CSS coordinate space to topWin's
  const winToTopWinCssScale = win.devicePixelRatio / topWin.devicePixelRatio;
  screenX = screenX * winToTopWinCssScale;
  screenY = screenY * winToTopWinCssScale;

  doc = topWin.document;

  let popupset = doc.querySelector("popupset");
  if (!popupset) {
    popupset = doc.createXULElement("popupset");
    doc.documentElement.appendChild(popupset);
  }

  const popup = doc.createXULElement("menupopup");
  popup.setAttribute("menu-api", "true");
  popup.setAttribute("consumeoutsideclicks", "false");
  popup.setAttribute("incontentshell", "false");

  if (this.id) {
    popup.id = this.id;
  }
  this._createMenuItems(popup);

  // The context menu will be created in the topmost chrome window. Hide it manually when
  // the owner document is unloaded.
  const onWindowUnload = () => popup.hidePopup();
  win.addEventListener("unload", onWindowUnload);

  // Remove the menu from the DOM once it's hidden.
  popup.addEventListener("popuphidden", e => {
    if (e.target === popup) {
      win.removeEventListener("unload", onWindowUnload);
      popup.remove();
      this.emit("close");
    }
  });

  popup.addEventListener("popupshown", e => {
    if (e.target === popup) {
      this.emit("open");
    }
  });

  popupset.appendChild(popup);
  popup.openPopupAtScreen(screenX, screenY, true);
};

Menu.prototype._createMenuItems = function (parent) {
  const doc = parent.ownerDocument;
  this.menuitems.forEach(item => {
    if (!item.visible) {
      return;
    }

    if (item.submenu) {
      const menupopup = doc.createXULElement("menupopup");
      menupopup.setAttribute("incontentshell", "false");

      item.submenu._createMenuItems(menupopup);

      const menu = doc.createXULElement("menu");
      menu.appendChild(menupopup);
      applyItemAttributesToNode(item, menu);
      parent.appendChild(menu);
    } else if (item.type === "separator") {
      const menusep = doc.createXULElement("menuseparator");
      parent.appendChild(menusep);
    } else {
      const menuitem = doc.createXULElement("menuitem");
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

Menu.getMenuElementById = function (id, doc) {
  const menuDoc = DevToolsUtils.getTopWindow(doc.defaultView).document;
  return menuDoc.getElementById(id);
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
    node.ownerDocument.l10n.setAttributes(node, item.l10nID);
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
  if (item.image) {
    node.setAttribute("image", item.image);
  }
  if (item.id) {
    node.id = item.id;
  }
}

module.exports = Menu;
