/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { L10N } = require("../utils/l10n");

loader.lazyRequireGetter(this, "copyString", "devtools/shared/platform/clipboard", true);
loader.lazyRequireGetter(this, "showMenu", "devtools/client/shared/components/menu/utils", true);

class PropertiesViewContextMenu {
  constructor(props) {
    this.props = props;
  }

  /**
   * Handle the context menu opening.
   * @param {*} event open event
   * @param {*} member member of the right-clicked row
   * @param {*} object the whole data object
   */
  open(event = {}, { member, object }) {
    const menu = [];

    menu.push({
      id: "properties-view-context-menu-copy",
      label: L10N.getStr("netmonitor.context.copy"),
      accesskey: L10N.getStr("netmonitor.context.copy.accesskey"),
      click: () => this.copySelected(member),
    });

    menu.push({
      id: "properties-view-context-menu-copyall",
      label: L10N.getStr("netmonitor.context.copyAll"),
      accesskey: L10N.getStr("netmonitor.context.copyAll.accesskey"),
      click: () => this.copyAll(object),
    });

    showMenu(menu, {
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }

  copyAll(data) {
    try {
      copyString(JSON.stringify(data));
    } catch (error) {}
  }

  copySelected({ object, hasChildren }) {
    if (hasChildren) {
      // If has children, copy the data as JSON
      try {
        copyString(JSON.stringify({ [object.name]: object.value }));
      } catch (error) {}
    } else {
      // Copy the data as key-value format
      copyString(`${object.name}: ${object.value}`);
    }
  }
}

module.exports = PropertiesViewContextMenu;
