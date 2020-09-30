/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import Services from "devtools-services";

const { appinfo } = Services;

const isMacOS = appinfo.OS === "Darwin";

const EventEmitter = require("../utils/event-emitter");

/**
 * Formats key for use in tooltips
 * For macOS we use the following unicode
 *
 * cmd ⌘ = \u2318
 * shift ⇧ – \u21E7
 * option (alt) ⌥ \u2325
 *
 * For Win/Lin this replaces CommandOrControl or CmdOrCtrl with Ctrl
 *
 * @static
 */
function formatKeyShortcut(shortcut) {
    if (isMacOS) {
        return shortcut
            .replace(/Shift\+/g, "\u21E7")
            .replace(/Command\+|Cmd\+/g, "\u2318")
            .replace(/CommandOrControl\+|CmdOrCtrl\+/g, "\u2318")
            .replace(/Alt\+/g, "\u2325");
    }
    return shortcut
        .replace(/CommandOrControl\+|CmdOrCtrl\+/g, `${L10N.getStr("ctrl")}+`)
        .replace(/Shift\+/g, "Shift+");
}

function inToolbox() {
  try {
    return window.parent.document.documentURI.startsWith("about:devtools-toolbox");
  } catch (e) {
    // If `window` is not available, it's very likely that we are in the toolbox.
    return true;
  }
}

// Copied from m-c DevToolsUtils.
function getTopWindow(win) {
  return win.windowRoot ? win.windowRoot.ownerGlobal : win.top;
}

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
Menu.prototype.append = function (menuItem) {
  this.menuitems.push(menuItem);
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
  // The context-menu will be created in the topmost window to preserve keyboard
  // navigation. See Bug 1543940. Keep a reference on the window owning the menu to hide
  // the popup on unload.
  const win = doc.defaultView;
  doc = getTopWindow(doc.defaultView).document;

  let popupset = doc.querySelector("popupset");
  if (!popupset) {
    popupset = doc.createXULElement("popupset");
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

  popup = this.createPopup(doc);
  popup.setAttribute("menu-api", "true");

  if (this.id) {
    popup.id = this.id;
  }
  this._createMenuItems(popup);

  // The context menu will be created in the topmost chrome window. Hide it manually when
  // the owner document is unloaded.
  const onWindowUnload = () => popup.hidePopup();
  win.addEventListener("unload", onWindowUnload);

  // Remove the menu from the DOM once it's hidden.
  popup.addEventListener("popuphidden", (e) => {
    if (e.target === popup) {
      win.removeEventListener("unload", onWindowUnload);
      popup.remove();
      this.emit("close", popup);
    }
  });

  popup.addEventListener("popupshown", (e) => {
    if (e.target === popup) {
      this.emit("open", popup);
    }
  });

  popupset.appendChild(popup);
  popup.openPopupAtScreen(screenX, screenY, true);
};

Menu.prototype.createPopup = function(doc) {
  return doc.createElement("menupopup");
}

Menu.prototype._createMenuItems = function(parent) {
  let doc = parent.ownerDocument;
  this.menuitems.forEach(item => {
    if (!item.visible) {
      return;
    }

    if (item.submenu) {
      let menupopup = doc.createElement("menupopup");
      item.submenu._createMenuItems(menupopup);

      let menuitem = doc.createElement("menuitem");
      menuitem.setAttribute("label", item.label);
      if (!inToolbox()) {
        menuitem.textContent = item.label;
      }

      let menu = doc.createElement("menu");
      menu.appendChild(menuitem);
      menu.appendChild(menupopup);
      if (item.disabled) {
        menu.setAttribute("disabled", "true");
      }
      if (item.accesskey) {
        menu.setAttribute("accesskey", item.accesskey);
      }
      if (item.id) {
        menu.id = item.id;
      }
      if (item.accelerator) {
        menuitem.setAttribute("acceltext", formatKeyShortcut(item.accelerator));
      }
      parent.appendChild(menu);
    } else if (item.type === "separator") {
      let menusep = doc.createElement("menuseparator");
      parent.appendChild(menusep);
    } else {
      let menuitem = doc.createElement("menuitem");
      menuitem.setAttribute("label", item.label);

      if (!inToolbox()) {
        menuitem.textContent = item.label;
      }

      menuitem.addEventListener("command", () => item.click());

      if (item.type === "checkbox") {
        menuitem.setAttribute("type", "checkbox");
      }
      if (item.type === "radio") {
        menuitem.setAttribute("type", "radio");
      }
      if (item.disabled) {
        menuitem.setAttribute("disabled", "true");
      }
      if (item.checked) {
        menuitem.setAttribute("checked", "true");
      }
      if (item.accesskey) {
        menuitem.setAttribute("accesskey", item.accesskey);
      }
      if (item.id) {
        menuitem.id = item.id;
      }
      if (item.accelerator) {
        menuitem.setAttribute("acceltext", formatKeyShortcut(item.accelerator));
      }
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

module.exports = Menu;
