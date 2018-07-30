/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const SidebarItem = createFactory(require("./SidebarItem"));

const FIREFOX_ICON = "chrome://devtools/skin/images/firefox-logo-glyph.svg";

class Sidebar extends PureComponent {
  render() {
    return dom.section(
      {
        className: "sidebar",
      },
      dom.ul(
        {},
        SidebarItem({
          icon: FIREFOX_ICON,
          isSelected: true,
          name: "This Firefox",
        })
      )
    );
  }
}

module.exports = Sidebar;
