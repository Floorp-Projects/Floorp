/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global React */

"use strict";

loader.lazyRequireGetter(this, "React",
  "devtools/client/shared/vendor/react");
loader.lazyRequireGetter(this, "TabMenuEntry",
  "devtools/client/aboutdebugging/components/tab-menu-entry", true);

exports.TabMenu = React.createClass({
  displayName: "TabMenu",

  render() {
    let { tabs, selectedTabId, selectTab } = this.props;
    let tabLinks = tabs.map(({ id, name, icon }) => {
      let selected = id == selectedTabId;
      return React.createElement(TabMenuEntry,
        { tabId: id, name, icon, selected, selectTab });
    });

    // "categories" id used for styling purposes
    return React.createElement("div", { id: "categories" }, tabLinks);
  },
});
