/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HEADERS } = require("./constants");
const { L10N } = require("./utils/l10n");
const { showMenu } = require("./utils/menu");

const stringMap = HEADERS
  .filter((header) => header.hasOwnProperty("label"))
  .reduce((acc, { name, label }) => Object.assign(acc, { [name]: label }), {});

class RequestListHeaderContextMenu {
  constructor({ toggleColumn, resetColumns }) {
    this.toggleColumn = toggleColumn;
    this.resetColumns = resetColumns;
  }

  get columns() {
    // FIXME: Bug 1362059 - Implement RequestListHeaderContextMenu React component
    // Remove window.store
    return window.store.getState().ui.columns;
  }

  get visibleColumns() {
    return [...this.columns].filter(([_, shown]) => shown);
  }

  /**
   * Handle the context menu opening.
   */
  open(event = {}) {
    let menu = [];
    let onlyOneColumn = this.visibleColumns.length === 1;

    for (let [column, shown] of this.columns) {
      menu.push({
        id: `request-list-header-${column}-toggle`,
        label: L10N.getStr(`netmonitor.toolbar.${stringMap[column] || column}`),
        type: "checkbox",
        checked: shown,
        click: () => this.toggleColumn(column),
        // We don't want to allow hiding the last visible column
        disabled: onlyOneColumn && shown,
      });
    }

    menu.push({ type: "separator" });

    menu.push({
      id: "request-list-header-reset-columns",
      label: L10N.getStr("netmonitor.toolbar.resetColumns"),
      click: () => this.resetColumns(),
    });

    return showMenu(event, menu);
  }
}

module.exports = RequestListHeaderContextMenu;
