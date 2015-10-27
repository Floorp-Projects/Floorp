/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const INDENT = 10;

/**
 * An arrow that displays whether its node is expanded (▼) or collapsed
 * (▶). When its node has no children, it is hidden.
 */
const TreeItem = module.exports = createClass({
  displayName: "tree-item",

  render() {
    let { item, depth, arrow, focused } = this.props;

    return dom.div({ className: "heap-tree-item" },
      dom.span({ className: "heap-tree-item-bytes" }, item.bytes),
      dom.span({ className: "heap-tree-item-count" }, item.count),
      dom.span({ className: "heap-tree-item-total-bytes" }, item.totalBytes),
      dom.span({ className: "heap-tree-item-total-count" }, item.totalCount),
      dom.span({ className: "heap-tree-item-name", style: { marginLeft: depth * INDENT }},
        arrow,
        item.name
      )
    );
  }
});
