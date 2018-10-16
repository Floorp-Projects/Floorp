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
      children: PropTypes.arrayOf(PropTypes.element).isRequired,
      className: PropTypes.string,
      isSelected: PropTypes.bool.isRequired,
      selectable: PropTypes.bool.isRequired,
      onSelect: PropTypes.func.isRequired,
    };
  }

  onItemClick() {
    this.props.onSelect();
  }

  render() {
    const {children, className, isSelected, selectable } = this.props;

    return dom.li(
      {
        className: "sidebar-item js-sidebar-item" +
                   (className ? ` ${className}` : "") +
                   (isSelected ?
                      " sidebar-item--selected js-sidebar-item-selected" :
                      ""
                   ) +
                   (selectable ? " sidebar-item--selectable" : ""),
        onClick: selectable ? () => this.onItemClick() : null
      },
      children
    );
  }
}

module.exports = SidebarItem;
