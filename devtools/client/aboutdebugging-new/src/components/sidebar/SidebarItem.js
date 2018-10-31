/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * This component is used as a wrapper by items in the sidebar.
 */
class SidebarItem extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node.isRequired,
      className: PropTypes.string,
      isSelected: PropTypes.bool.isRequired,
      selectable: PropTypes.bool.isRequired,
      // only require `onSelect` function when `selectable` is true
      onSelect: (props, propName, componentName) => {
        const isFn = props[propName] && typeof props[propName] === "function";
        if (props.selectable && !isFn) {
          return new Error(`Missing ${propName} function supplied to ${componentName}. ` +
            "(you must set this prop when selectable is true)");
        }
        return null; // for eslint (consistent-return rule)
      },
    };
  }

  // temporary handler until a router is in place
  onItemClick(evt) {
    evt.preventDefault();
    this.props.onSelect();
  }

  renderContent() {
    const { children, selectable } = this.props;

    if (selectable) {
      return dom.a(
        {
          className: "sidebar-item__link js-sidebar-link",
          href: "#", // to be changed with a path when a router is in place
          onClick: (evt) => this.onItemClick(evt),
        },
        children
      );
    }

    return children;
  }

  render() {
    const {className, isSelected, selectable } = this.props;

    return dom.li(
      {
        className: "sidebar-item js-sidebar-item" +
                   (className ? ` ${className}` : "") +
                   (isSelected ?
                      " sidebar-item--selected js-sidebar-item-selected" :
                      ""
                   ) +
                   (selectable ? " sidebar-item--selectable" : ""),
      },
      this.renderContent()
    );
  }
}

module.exports = SidebarItem;
