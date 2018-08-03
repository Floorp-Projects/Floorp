/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { PAGES } = require("../constants");

const SidebarItem = createFactory(require("./SidebarItem"));
const FIREFOX_ICON = "chrome://devtools/skin/images/firefox-logo-glyph.svg";
const MOBILE_ICON = "chrome://devtools/skin/images/firefox-logo-glyph.svg";

class Sidebar extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      selectedPage: PropTypes.string.isRequired,
    };
  }

  render() {
    const { dispatch, selectedPage } = this.props;

    return dom.aside(
      {
        className: "sidebar",
      },
      dom.ul(
        {},
        SidebarItem({
          id: PAGES.THIS_FIREFOX,
          dispatch,
          icon: FIREFOX_ICON,
          isSelected: PAGES.THIS_FIREFOX === selectedPage,
          name: "This Firefox",
        }),
        SidebarItem({
          id: PAGES.CONNECT,
          dispatch,
          icon: MOBILE_ICON,
          isSelected: PAGES.CONNECT === selectedPage,
          name: "Connect",
        })
      )
    );
  }
}

module.exports = Sidebar;
