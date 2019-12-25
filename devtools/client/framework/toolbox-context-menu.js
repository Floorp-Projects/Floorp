/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");

// This WeakMap will be used to know if strings have already been loaded in a given
// window, which will be used as key.
const stringsLoaded = new WeakMap();

/**
 * Lazily load strings for the edit menu.
 */
function loadEditMenuStrings(win) {
  if (stringsLoaded.has(win)) {
    return;
  }

  if (win.MozXULElement) {
    stringsLoaded.set(win, true);
    win.MozXULElement.insertFTLIfNeeded("toolkit/main-window/editmenu.ftl");
  }
}

/**
 * Return an 'edit' menu for a input field. This integrates directly
 * with docshell commands to provide the right enabled state and editor
 * functionality.
 *
 * You'll need to call menu.popup() yourself, this just returns the Menu instance.
 *
 * @param {Window} win parent window reference
 * @param {String} id menu ID
 *
 * @returns {Menu}
 */
function createEditContextMenu(win, id) {
  // Localized strings for the menu are loaded lazily.
  loadEditMenuStrings(win);

  const docshell = win.docShell;
  const menu = new Menu({ id });
  menu.append(
    new MenuItem({
      id: "editmenu-undo",
      l10nID: "editmenu-undo",
      disabled: !docshell.isCommandEnabled("cmd_undo"),
      click: () => {
        docshell.doCommand("cmd_undo");
      },
    })
  );
  menu.append(
    new MenuItem({
      type: "separator",
    })
  );
  menu.append(
    new MenuItem({
      id: "editmenu-cut",
      l10nID: "editmenu-cut",
      disabled: !docshell.isCommandEnabled("cmd_cut"),
      click: () => {
        docshell.doCommand("cmd_cut");
      },
    })
  );
  menu.append(
    new MenuItem({
      id: "editmenu-copy",
      l10nID: "editmenu-copy",
      disabled: !docshell.isCommandEnabled("cmd_copy"),
      click: () => {
        docshell.doCommand("cmd_copy");
      },
    })
  );
  menu.append(
    new MenuItem({
      id: "editmenu-paste",
      l10nID: "editmenu-paste",
      disabled: !docshell.isCommandEnabled("cmd_paste"),
      click: () => {
        docshell.doCommand("cmd_paste");
      },
    })
  );
  menu.append(
    new MenuItem({
      id: "editmenu-delete",
      l10nID: "editmenu-delete",
      disabled: !docshell.isCommandEnabled("cmd_delete"),
      click: () => {
        docshell.doCommand("cmd_delete");
      },
    })
  );
  menu.append(
    new MenuItem({
      type: "separator",
    })
  );
  menu.append(
    new MenuItem({
      id: "editmenu-selectAll",
      l10nID: "editmenu-select-all",
      disabled: !docshell.isCommandEnabled("cmd_selectAll"),
      click: () => {
        docshell.doCommand("cmd_selectAll");
      },
    })
  );
  return menu;
}

module.exports.createEditContextMenu = createEditContextMenu;
