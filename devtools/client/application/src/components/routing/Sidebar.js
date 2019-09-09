/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const {
  aside,
  ul,
} = require("devtools/client/shared/vendor/react-dom-factories");

const SidebarItem = createFactory(require("./SidebarItem"));

const { PAGE_TYPES } = require("../../constants");

class Sidebar extends PureComponent {
  render() {
    const navItems = Object.values(PAGE_TYPES);

    return aside(
      {
        className: "sidebar",
      },
      ul(
        {
          className: "sidebar__list",
        },
        navItems.map(page => {
          return SidebarItem({
            page: page,
            key: `sidebar-item-${page}`,
          });
        })
      )
    );
  }
}

// Exports
module.exports = Sidebar;
