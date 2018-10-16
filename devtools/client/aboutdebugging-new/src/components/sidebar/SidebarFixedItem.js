/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const SidebarItem = createFactory(require("./SidebarItem"));

const Actions = require("../../actions/index");

/**
 * This component displays a fixed item in the Sidebar component.
 */
class SidebarFixedItem extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      icon: PropTypes.string.isRequired,
      id: PropTypes.string.isRequired,
      isSelected: PropTypes.bool.isRequired,
      name: PropTypes.string.isRequired,
      runtimeId: PropTypes.string,
    };
  }

  render() {
    const {
      dispatch,
      id,
      icon,
      isSelected,
      name,
      runtimeId,
    } = this.props;

    return SidebarItem(
      {
        isSelected,
        selectable: true,
        className: "sidebar-fixed-item",
        onSelect: () => {
          dispatch(Actions.selectPage(id, runtimeId));
        }
      },
      dom.img(
        {
          className: "sidebar-fixed-item__icon",
          src: icon,
        }
      ),
      dom.span(
        {
          className: "ellipsis-text",
          title: name,
        },
        name
      )
    );
  }
}

module.exports = SidebarFixedItem;
