/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes, createFactory } = require("devtools/client/shared/vendor/react");
const { safeErrorString } = require("devtools/shared/DevToolsUtils");
const Tree = createFactory(require("./tree"));
const TreeItem = createFactory(require("./tree-item"));
const { getSnapshotStatusTextFull, L10N } = require("../utils");
const { snapshotState: states } = require("../constants");
const { snapshot: snapshotModel } = require("../models");
// If HEAP_TREE_ROW_HEIGHT changes, be sure to change `var(--heap-tree-row-height)`
// in `devtools/client/themes/memory.css`
const HEAP_TREE_ROW_HEIGHT = 14;

/**
 * Creates a hash map mapping node IDs to its parent node.
 *
 * @param {CensusTreeNode} node
 * @param {Object<number, CensusTreeNode>} aggregator
 *
 * @return {Object<number, CensusTreeNode>}
 */
function createParentMap (node, aggregator=Object.create(null)) {
  for (let child of (node.children || [])) {
    aggregator[child.id] = node;
    createParentMap(child, aggregator);
  }

  return aggregator;
}

/**
 * Creates properties to be passed into the Tree component.
 *
 * @param {CensusTreeNode} census
 * @return {Object}
 */
function createTreeProperties (census, toolbox) {
  let map = createParentMap(census);

  return {
    getParent: node => map[node.id],
    getChildren: node => node.children || [],
    renderItem: (item, depth, focused, arrow) => new TreeItem({ toolbox, item, depth, focused, arrow }),
    getRoots: () => census.children,
    getKey: node => node.id,
    itemHeight: HEAP_TREE_ROW_HEIGHT,
    // Because we never add or remove children when viewing the same census, we
    // can always reuse a cached traversal if one is available.
    reuseCachedTraversal: traversal => true,
  };
}

/**
 * Main view for the memory tool -- contains several panels for different states;
 * an initial state of only a button to take a snapshot, loading states, and the
 * heap view tree.
 */

const Heap = module.exports = createClass({
  displayName: "heap-view",

  propTypes: {
    onSnapshotClick: PropTypes.func.isRequired,
    snapshot: snapshotModel,
  },

  render() {
    let { snapshot, onSnapshotClick, toolbox } = this.props;
    let census = snapshot ? snapshot.census : null;
    let state = snapshot ? snapshot.state : "initial";
    let statusText = snapshot ? getSnapshotStatusTextFull(snapshot) : "";
    let content;

    switch (state) {
      case "initial":
        content = [dom.button({
          className: "devtools-toolbarbutton take-snapshot",
          onClick: onSnapshotClick,
          // Want to use the [standalone] tag to leverage our styles,
          // but React hates that evidently
          "data-standalone": true,
          "data-text-only": true,
        }, L10N.getStr("take-snapshot"))];
        break;
      case states.ERROR:
        content = [
          dom.span({ className: "snapshot-status error" }, statusText),
          dom.pre({}, safeErrorString(snapshot.error))
        ];
        break;
      case states.SAVING:
      case states.SAVED:
      case states.READING:
      case states.READ:
      case states.SAVING_CENSUS:
        content = [dom.span({ className: "snapshot-status devtools-throbber" }, statusText)];
        break;
      case states.SAVED_CENSUS:
        content = [
          dom.div({ className: "header" },
            dom.span({ className: "heap-tree-item-bytes" }, L10N.getStr("heapview.field.bytes")),
            dom.span({ className: "heap-tree-item-count" }, L10N.getStr("heapview.field.count")),
            dom.span({ className: "heap-tree-item-total-bytes" }, L10N.getStr("heapview.field.totalbytes")),
            dom.span({ className: "heap-tree-item-total-count" }, L10N.getStr("heapview.field.totalcount")),
            dom.span({ className: "heap-tree-item-name" }, L10N.getStr("heapview.field.name"))
          ),
          Tree(createTreeProperties(snapshot.census, toolbox))
        ];
        break;
    }
    let pane = dom.div({ className: "heap-view-panel", "data-state": state }, ...content);

    return (
      dom.div({ id: "heap-view", "data-state": state }, pane)
    )
  }
});
