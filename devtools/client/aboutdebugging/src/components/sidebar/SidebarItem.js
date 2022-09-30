/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const Link = createFactory(
  require("resource://devtools/client/shared/vendor/react-router-dom.js").Link
);

/**
 * This component is used as a wrapper by items in the sidebar.
 */
class SidebarItem extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node.isRequired,
      className: PropTypes.string,
      isSelected: PropTypes.bool.isRequired,
      to: PropTypes.string,
    };
  }

  static get defaultProps() {
    return {
      isSelected: false,
    };
  }

  renderContent() {
    const { children, to } = this.props;

    if (to) {
      const isExternalUrl = /^http/.test(to);

      return isExternalUrl
        ? dom.a(
            {
              className: "sidebar-item__link undecorated-link",
              href: to,
              target: "_blank",
            },
            children
          )
        : Link(
            {
              className: "sidebar-item__link qa-sidebar-link undecorated-link",
              to,
            },
            children
          );
    }

    return children;
  }

  render() {
    const { className, isSelected, to } = this.props;

    return dom.li(
      {
        className:
          "sidebar-item qa-sidebar-item" +
          (className ? ` ${className}` : "") +
          (isSelected
            ? " sidebar-item--selected qa-sidebar-item-selected"
            : "") +
          (to ? " sidebar-item--selectable" : ""),
      },
      this.renderContent()
    );
  }
}

module.exports = SidebarItem;
