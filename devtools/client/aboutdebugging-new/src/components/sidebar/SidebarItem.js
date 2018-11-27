/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Link = createFactory(require("devtools/client/shared/vendor/react-router-dom").Link);

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

  renderContent() {
    const { children, to } = this.props;

    if (to) {
      return Link(
        {
          className: "sidebar-item__link js-sidebar-link",
          to,
        },
        children
      );
    }

    return children;
  }

  render() {
    const {className, isSelected, to } = this.props;

    return dom.li(
      {
        className: "sidebar-item js-sidebar-item" +
                   (className ? ` ${className}` : "") +
                   (isSelected ?
                      " sidebar-item--selected js-sidebar-item-selected" :
                      ""
                   ) +
                   (to ? " sidebar-item--selectable" : ""),
      },
      this.renderContent()
    );
  }
}

module.exports = SidebarItem;
