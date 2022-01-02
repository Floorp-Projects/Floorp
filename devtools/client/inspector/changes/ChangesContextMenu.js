/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Menu = require("devtools/client/framework/menu");
loader.lazyRequireGetter(
  this,
  "MenuItem",
  "devtools/client/framework/menu-item"
);

const { getStr } = require("devtools/client/inspector/changes/utils/l10n");

/**
 * Context menu for the Changes panel with options to select, copy and export CSS changes.
 */
class ChangesContextMenu extends Menu {
  constructor(config = {}) {
    super(config);
    this.onCopy = config.onCopy;
    this.onCopyAllChanges = config.onCopyAllChanges;
    this.onCopyDeclaration = config.onCopyDeclaration;
    this.onCopyRule = config.onCopyRule;
    this.onSelectAll = config.onSelectAll;
    this.toolboxDocument = config.toolboxDocument;
    this.window = config.window;
  }

  show(event) {
    this._openMenu({
      target: event.target,
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }

  _openMenu({ target, screenX = 0, screenY = 0 } = {}) {
    this.window.focus();
    // Remove existing menu items.
    this.clear();

    // Copy option
    const menuitemCopy = new MenuItem({
      id: "changes-contextmenu-copy",
      label: getStr("changes.contextmenu.copy"),
      accesskey: getStr("changes.contextmenu.copy.accessKey"),
      click: this.onCopy,
      disabled: !this._hasTextSelected(),
    });
    this.append(menuitemCopy);

    const declEl = target.closest(".changes__declaration");
    const ruleEl = target.closest("[data-rule-id]");
    const ruleId = ruleEl ? ruleEl.dataset.ruleId : null;

    if (ruleId || declEl) {
      // Copy Rule option
      this.append(
        new MenuItem({
          id: "changes-contextmenu-copy-rule",
          label: getStr("changes.contextmenu.copyRule"),
          click: () => this.onCopyRule(ruleId, true),
        })
      );

      // Copy Declaration option. Visible only if there is a declaration element target.
      this.append(
        new MenuItem({
          id: "changes-contextmenu-copy-declaration",
          label: getStr("changes.contextmenu.copyDeclaration"),
          click: () => this.onCopyDeclaration(declEl),
          visible: !!declEl,
        })
      );

      this.append(
        new MenuItem({
          type: "separator",
        })
      );
    }

    // Select All option
    const menuitemSelectAll = new MenuItem({
      id: "changes-contextmenu-select-all",
      label: getStr("changes.contextmenu.selectAll"),
      accesskey: getStr("changes.contextmenu.selectAll.accessKey"),
      click: this.onSelectAll,
    });
    this.append(menuitemSelectAll);

    this.popup(screenX, screenY, this.toolboxDocument);
  }

  _hasTextSelected() {
    const selection = this.window.getSelection();
    return selection.toString() && !selection.isCollapsed;
  }

  destroy() {
    this.window = null;
    this.toolboxDocument = null;
  }
}

module.exports = ChangesContextMenu;
