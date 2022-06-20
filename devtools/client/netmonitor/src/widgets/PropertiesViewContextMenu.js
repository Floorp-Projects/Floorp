/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  contextMenuFormatters,
} = require("devtools/client/netmonitor/src/utils/context-menu-utils");

loader.lazyRequireGetter(
  this,
  "copyString",
  "devtools/shared/platform/clipboard",
  true
);
loader.lazyRequireGetter(
  this,
  "showMenu",
  "devtools/client/shared/components/menu/utils",
  true
);

class PropertiesViewContextMenu {
  constructor(props = {}) {
    this.props = props;
    this.copyAll = this.copyAll.bind(this);
    this.copyValue = this.copyValue.bind(this);
  }

  /**
   * Handle the context menu opening.
   * @param {Object} event open event
   * @param {Object} selection object representing the current selection
   * @param {Object} data object containing information
   * @param {Object} data.member member of the right-clicked row
   * @param {Object} data.object the whole tree data
   */
  open(event = {}, selection, { member, object }) {
    const menuItems = [
      {
        id: "properties-view-context-menu-copyvalue",
        label: L10N.getStr("netmonitor.context.copyValue"),
        accesskey: L10N.getStr("netmonitor.context.copyValue.accesskey"),
        click: () => this.copyValue(member, selection),
      },
      {
        id: "properties-view-context-menu-copyall",
        label: L10N.getStr("netmonitor.context.copyAll"),
        accesskey: L10N.getStr("netmonitor.context.copyAll.accesskey"),
        click: () => this.copyAll(object, selection),
      },
    ];

    showMenu(menuItems, {
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }

  /**
   * Copies all.
   * @param {Object} object the whole tree data
   * @param {Object} selection object representing the current selection
   */
  copyAll(object, selection) {
    let buffer = "";
    if (selection.toString() !== "") {
      buffer = selection.toString();
    } else {
      const { customFormatters } = this.props;
      buffer = contextMenuFormatters.baseCopyAllFormatter(object);
      if (customFormatters?.copyAllFormatter) {
        buffer = customFormatters.copyAllFormatter(
          object,
          contextMenuFormatters.baseCopyAllFormatter
        );
      }
    }
    try {
      copyString(buffer);
    } catch (error) {}
  }

  /**
   * Copies the value of a single item.
   * @param {Object} member member of the right-clicked row
   * @param {Object} selection object representing the current selection
   */
  copyValue(member, selection) {
    let buffer = "";
    if (selection.toString() !== "") {
      buffer = selection.toString();
    } else {
      const { customFormatters } = this.props;
      buffer = contextMenuFormatters.baseCopyFormatter(member);
      if (customFormatters?.copyFormatter) {
        buffer = customFormatters.copyFormatter(
          member,
          contextMenuFormatters.baseCopyFormatter
        );
      }
    }
    try {
      copyString(buffer);
    } catch (error) {}
  }
}

module.exports = PropertiesViewContextMenu;
