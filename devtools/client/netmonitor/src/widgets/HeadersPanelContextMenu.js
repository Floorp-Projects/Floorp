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

class HeadersPanelContextMenu {
  constructor(props = {}) {
    this.props = props;
    this.copyAll = this.copyAll.bind(this);
    this.copy = this.copy.bind(this);
  }

  /**
   * Handle the context menu opening.
   * @param {Object} event open event
   * @param {Object} selection object representing the current selection
   */
  open(event = {}, selection) {
    const { target } = event;
    const menuItems = [
      {
        id: "headers-panel-context-menu-copy",
        label: L10N.getStr("netmonitor.context.copy"),
        accesskey: L10N.getStr("netmonitor.context.copy.accesskey"),
        click: () => {
          const { name, value } = getSummaryContent(
            target.closest(".tabpanel-summary-container")
          );
          this.copy(
            { name, value, object: null, hasChildren: false },
            selection
          );
        },
      },
      {
        id: "headers-panel-context-menu-copyall",
        label: L10N.getStr("netmonitor.context.copyAll"),
        accesskey: L10N.getStr("netmonitor.context.copyAll.accesskey"),
        click: () => {
          const root = target.closest(".summary");
          const object = {};
          if (root) {
            const { children } = root;
            for (let i = 0; i < children.length; i++) {
              const content = getSummaryContent(children[i]);
              object[content.name] = content.value;
            }
          }
          return this.copyAll(object, selection);
        },
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
   * Copies single item.
   * @param {Object} object data object for specific node
   * @param {Object} selection object representing the current selection
   */
  copy(object, selection) {
    let buffer = "";
    if (selection.toString() !== "") {
      buffer = selection.toString();
    } else {
      const { customFormatters } = this.props;
      buffer = contextMenuFormatters.baseCopyFormatter(object);
      if (customFormatters?.copyFormatter) {
        buffer = customFormatters.copyFormatter(
          object,
          contextMenuFormatters.baseCopyFormatter
        );
      }
    }
    try {
      copyString(buffer);
    } catch (error) {}
  }
}

function getSummaryContent(el) {
  return {
    name: el.querySelector(".tabpanel-summary-label").textContent,
    value: el.querySelector(".tabpanel-summary-value").textContent,
  };
}

module.exports = HeadersPanelContextMenu;
