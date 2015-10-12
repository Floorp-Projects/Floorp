/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes, createFactory } = require("devtools/client/shared/vendor/react");
const Tree = createFactory(require("./tree"));
const TreeItem = createFactory(require("./tree-item"));
const { getSnapshotStatusText } = require("../utils");
const { snapshotState: states } = require("../constants");
const { snapshot: snapshotModel } = require("../models");
const TAKE_SNAPSHOT_TEXT = "Take snapshot";
const TREE_ROW_HEIGHT = 10;

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
function createTreeProperties (census) {
  let map = createParentMap(census);

  return {
    // getParent only used for focusing parents when child selected;
    // handle this later?
    getParent: node => map(node.id),
    getChildren: node => node.children || [],
    renderItem: (item, depth, focused, arrow) => new TreeItem({ item, depth, focused, arrow }),
    getRoots: () => census.children,
    getKey: node => node.id,
    itemHeight: TREE_ROW_HEIGHT,
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
    let { snapshot, onSnapshotClick } = this.props;
    let pane;
    let census = snapshot ? snapshot.census : null;
    let state = snapshot ? snapshot.state : "initial";

    switch (state) {
      case "initial":
        pane = dom.div({ className: "heap-view-panel", "data-state": "initial" },
          dom.button({ className: "take-snapshot", onClick: onSnapshotClick }, TAKE_SNAPSHOT_TEXT)
        );
        break;
      case states.SAVING:
      case states.SAVED:
      case states.READING:
      case states.READ:
      case states.SAVING_CENSUS:
        pane = dom.div({ className: "heap-view-panel", "data-state": state },
          getSnapshotStatusText(snapshot));
        break;
      case states.SAVED_CENSUS:
        pane = dom.div({ className: "heap-view-panel", "data-state": "loaded" },
          Tree(createTreeProperties(snapshot.census))
        );
        break;
    }

    return (
      dom.div({ id: "heap-view", "data-state": state }, pane)
    )
  }
});
