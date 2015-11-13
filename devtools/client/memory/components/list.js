/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

/**
 * Generic list component that takes another react component to represent
 * the children nodes as `itemComponent`, and a list of items to render
 * as that component with a click handler.
 */
const List = module.exports = createClass({
  displayName: "list",

  propTypes: {
    itemComponent: PropTypes.any.isRequired,
    onClick: PropTypes.func,
    items: PropTypes.array.isRequired,
  },

  render() {
    let { items, onClick, itemComponent: Item } = this.props;

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
});
