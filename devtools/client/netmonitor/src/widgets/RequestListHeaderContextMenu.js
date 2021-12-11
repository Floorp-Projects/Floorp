/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { showMenu } = require("devtools/client/shared/components/menu/utils");
const { HEADERS } = require("devtools/client/netmonitor/src/constants");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  getVisibleColumns,
} = require("devtools/client/netmonitor/src/selectors/index");

const stringMap = HEADERS.filter(header =>
  header.hasOwnProperty("label")
).reduce((acc, { name, label }) => Object.assign(acc, { [name]: label }), {});

const subMenuMap = HEADERS.filter(header =>
  header.hasOwnProperty("subMenu")
).reduce(
  (acc, { name, subMenu }) => Object.assign(acc, { [name]: subMenu }),
  {}
);

const nonLocalizedHeaders = HEADERS.filter(header =>
  header.hasOwnProperty("noLocalization")
).map(header => header.name);

class RequestListHeaderContextMenu {
  constructor(props) {
    this.props = props;
  }

  /**
   * Handle the context menu opening.
   */
  open(event = {}, columns) {
    const menu = [];
    const subMenu = { timings: [], responseHeaders: [] };
    const onlyOneColumn = getVisibleColumns(columns).length === 1;

    for (const column in columns) {
      const shown = columns[column];
      const label = nonLocalizedHeaders.includes(column)
        ? stringMap[column] || column
        : L10N.getStr(`netmonitor.toolbar.${stringMap[column] || column}`);
      const entry = {
        id: `request-list-header-${column}-toggle`,
        label,
        type: "checkbox",
        checked: shown,
        click: () => this.props.toggleColumn(column),
        // We don't want to allow hiding the last visible column
        disabled: onlyOneColumn && shown,
      };
      subMenuMap.hasOwnProperty(column)
        ? subMenu[subMenuMap[column]].push(entry)
        : menu.push(entry);
    }

    menu.push({ type: "separator" });
    menu.push({
      label: L10N.getStr("netmonitor.toolbar.timings"),
      submenu: subMenu.timings,
    });
    menu.push({
      label: L10N.getStr("netmonitor.toolbar.responseHeaders"),
      submenu: subMenu.responseHeaders,
    });

    menu.push({ type: "separator" });
    menu.push({
      id: "request-list-header-reset-columns",
      label: L10N.getStr("netmonitor.toolbar.resetColumns"),
      click: () => this.props.resetColumns(),
    });

    menu.push({
      id: "request-list-header-reset-sorting",
      label: L10N.getStr("netmonitor.toolbar.resetSorting"),
      click: () => this.props.resetSorting(),
    });

    const columnName = event.target.getAttribute("data-name");

    menu.push({
      id: "request-list-header-resize-column-to-fit-content",
      label: L10N.getStr("netmonitor.toolbar.resizeColumnToFitContent"),
      click: () => this.props.resizeColumnToFitContent(columnName),
    });

    showMenu(menu, {
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }
}

module.exports = RequestListHeaderContextMenu;
