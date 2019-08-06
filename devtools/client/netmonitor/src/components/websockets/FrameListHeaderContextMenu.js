/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { showMenu } = require("devtools/client/shared/components/menu/utils");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");

class FrameListHeaderContextMenu {
  constructor(props) {
    this.props = props;
  }

  /**
   * Handle the context menu opening.
   */
  open(event = {}, columns) {
    const visibleColumns = Object.values(columns).filter(state => state);
    const onlyOneColumn = visibleColumns.length === 1;

    const menuItems = Object.entries(columns).map(([column, shown]) => {
      const label = L10N.getStr(`netmonitor.ws.toolbar.${column}`);
      return {
        id: `ws-frames-list-header-${column}-toggle`,
        label,
        type: "checkbox",
        checked: shown,
        click: () => this.props.toggleColumn(column),
        // We don't want to allow hiding the last visible column
        disabled: onlyOneColumn && shown,
      };
    });
    menuItems.push(
      { type: "separator" },
      {
        id: "ws-frames-list-header-reset-columns",
        label: L10N.getStr("netmonitor.ws.toolbar.resetColumns"),
        click: () => this.props.resetColumns(),
      }
    );

    showMenu(menuItems, {
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }
}

module.exports = FrameListHeaderContextMenu;
