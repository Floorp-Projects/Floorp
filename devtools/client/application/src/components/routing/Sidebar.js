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
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { connect } = require("devtools/client/shared/vendor/react-redux");

const SidebarItem = createFactory(require("./SidebarItem"));

const { PAGE_TYPES } = require("../../constants");

class Sidebar extends PureComponent {
  static get propTypes() {
    return {
      // this prop is automatically injected via connect
      selectedPage: PropTypes.oneOf(Object.values(PAGE_TYPES)),
    };
  }

  render() {
    const navItems = Object.values(PAGE_TYPES);

    const isSelected = page => {
      return page === this.props.selectedPage;
    };

    return aside(
      {
        className: "sidebar js-sidebar",
      },
      ul(
        {
          className: "sidebar__list",
        },
        navItems.map(page => {
          return SidebarItem({
            page: page,
            key: `sidebar-item-${page}`,
            isSelected: isSelected(page),
          });
        })
      )
    );
  }
}

function mapStateToProps(state) {
  return {
    selectedPage: state.ui.selectedPage,
  };
}

module.exports = connect(mapStateToProps)(Sidebar);
