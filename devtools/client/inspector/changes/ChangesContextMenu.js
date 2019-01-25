/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "Menu", "devtools/client/framework/menu");
loader.lazyRequireGetter(this, "MenuItem", "devtools/client/framework/menu-item");
loader.lazyRequireGetter(this, "clipboardHelper", "devtools/shared/platform/clipboard");

const { getStr } = require("./utils/l10n");

/**
 * Context menu for the Changes panel with options to select, copy and export CSS changes.
 */
class ChangesContextMenu {
  /**
   * @param {ChangesView} view
   */
  constructor(view) {
    this.view = view;
    this.inspector = this.view.inspector;
    // Document object to which the Changes panel belongs to.
    this.document = this.view.document;
    // DOM element container for the Changes panel content.
    this.panel = this.document.getElementById("sidebar-panel-changes");
    // Window object to which the Changes panel belongs to.
    this.window = this.document.defaultView;

    this._onCopy = this._onCopy.bind(this);
    this._onSelectAll = this._onSelectAll.bind(this);
  }

  show(event) {
    this._openMenu({
      target: event.explicitOriginalTarget,
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }

  _openMenu({ target, screenX = 0, screenY = 0 } = {}) {
    this.window.focus();

    const menu = new Menu();

    // Copy option
    const menuitemCopy = new MenuItem({
      label: getStr("changes.contextmenu.copy"),
      accesskey: getStr("changes.contextmenu.copy.accessKey"),
      click: this._onCopy,
      disabled: !this._hasTextSelected(),
    });
    menu.append(menuitemCopy);

    // Select All option
    const menuitemSelectAll = new MenuItem({
      label: getStr("changes.contextmenu.selectAll"),
      accesskey: getStr("changes.contextmenu.selectAll.accessKey"),
      click: this._onSelectAll,
    });
    menu.append(menuitemSelectAll);

    menu.popup(screenX, screenY, this.inspector.toolbox);
    return menu;
  }

  _hasTextSelected() {
    const selection = this.window.getSelection();
    return selection.toString() && !selection.isCollapsed;
  }

  /**
   * Select all text.
   */
  _onSelectAll() {
    const selection = this.window.getSelection();
    selection.selectAllChildren(this.panel);
  }

  /**
   * Copy the selected text to clipboard.
   */
  _onCopy() {
    const text = this.window.getSelection().toString();
    clipboardHelper.copyString(text);
  }

  destroy() {
    this.inspector = null;
    this.panel = null;
    this.view = null;
    this.window = null;
  }
}

module.exports = ChangesContextMenu;
