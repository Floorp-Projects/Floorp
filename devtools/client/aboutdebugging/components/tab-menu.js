/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, createFactory, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const TabMenuEntry = createFactory(require("./tab-menu-entry"));

module.exports = createClass({
  displayName: "TabMenu",

  render() {
    let { tabs, selectedTabId, selectTab } = this.props;
    let tabLinks = tabs.map(({ panelId, id, name, icon }) => {
      let selected = id == selectedTabId;
      return TabMenuEntry({
        tabId: id, panelId, name, icon, selected, selectTab
      });
    });

    // "categories" id used for styling purposes
    return dom.div({ id: "categories", role: "tablist" }, tabLinks);
  },
});
