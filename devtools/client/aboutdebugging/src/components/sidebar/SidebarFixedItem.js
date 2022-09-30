/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const SidebarItem = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/sidebar/SidebarItem.js")
);

/**
 * This component displays a fixed item in the Sidebar component.
 */
class SidebarFixedItem extends PureComponent {
  static get propTypes() {
    return {
      icon: PropTypes.string.isRequired,
      isSelected: PropTypes.bool.isRequired,
      name: PropTypes.string.isRequired,
      to: PropTypes.string,
    };
  }

  render() {
    const { icon, isSelected, name, to } = this.props;

    return SidebarItem(
      {
        className: "sidebar-item--tall",
        isSelected,
        to,
      },
      dom.div(
        {
          className: "sidebar-fixed-item__container",
        },
        dom.img({
          className: "sidebar-fixed-item__icon",
          src: icon,
        }),
        dom.span(
          {
            className: "ellipsis-text",
            title: name,
          },
          name
        )
      )
    );
  }
}

module.exports = SidebarFixedItem;
