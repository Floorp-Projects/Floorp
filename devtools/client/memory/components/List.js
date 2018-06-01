/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * Generic list component that takes another react component to represent
 * the children nodes as `itemComponent`, and a list of items to render
 * as that component with a click handler.
 */
class List extends Component {
  static get propTypes() {
    return {
      itemComponent: PropTypes.any.isRequired,
      onClick: PropTypes.func,
      items: PropTypes.array.isRequired,
    };
  }

  render() {
    const { items, onClick, itemComponent: Item } = this.props;

    return (
      dom.ul({ className: "list" }, ...items.map((item, index) => {
        return Item(Object.assign({}, this.props, {
          key: index,
          item,
          index,
          onClick: () => onClick(item),
        }));
      }))
    );
  }
}

module.exports = List;
