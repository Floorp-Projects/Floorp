/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes, createFactory } = require("devtools/client/shared/vendor/react");
const { assert, safeErrorString } = require("devtools/shared/DevToolsUtils");
const Tree = createFactory(require("./tree"));
const TreeItem = createFactory(require("./tree-item"));
const { getStatusTextFull, L10N } = require("../utils");
const { snapshotState: states, diffingState } = require("../constants");
const { snapshot: snapshotModel, diffingModel } = require("../models");
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
 * @param {censusModel} census
 * @param {Object} toolbox
 * @param {Object} diffing
 * @return {Object}
 */
function createTreeProperties(census, toolbox, diffing) {
  const report = census.report;
  let map = createParentMap(report);
  const { totalBytes, totalCount } = report;

  const getPercentBytes = totalBytes === 0
    ? _ => 0
    : bytes => (bytes / totalBytes) * 100;

  const getPercentCount = totalCount === 0
    ? _ => 0
    : count => (count / totalCount) * 100;

  return {
    autoExpandDepth: 0,
    getParent: node => {
      const parent = map[node.id];
      return parent === report ? null : parent;
    },
    getChildren: node => node.children || [],
    renderItem: (item, depth, focused, arrow) =>
      new TreeItem({
        toolbox,
        item,
        depth,
        focused,
        arrow,
        getPercentBytes,
        getPercentCount,
        showSign: !!diffing,
      }),
    getRoots: () => report.children || [],
    getKey: node => node.id,
    itemHeight: HEAP_TREE_ROW_HEIGHT,
    // Because we never add or remove children when viewing the same census, we
    // can always reuse a cached traversal if one is available.
    reuseCachedTraversal: _ => true,
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
    toolbox: PropTypes.object.isRequired,
    diffing: diffingModel,
  },

  render() {
    let { snapshot, diffing, onSnapshotClick, toolbox } = this.props;

    let census;
    let state;
    let statusText;
    let error;
    if (diffing) {
      census = diffing.census;
      state = diffing.state;

      if (diffing.state === diffingState.SELECTING) {
        statusText = L10N.getStr(diffing.firstSnapshotId === null
                                   ? "diffing.prompt.selectBaseline"
                                   : "diffing.prompt.selectComparison");
      } else {
        statusText = getStatusTextFull(diffing.state);
      }

      if (diffing.error) {
        error = diffing.error;
      }
    } else {
      census = snapshot ? snapshot.census : null;
      state = snapshot ? snapshot.state : "initial";
      statusText = snapshot ? getStatusTextFull(snapshot.state) : "";
      if (snapshot && snapshot.error) {
        error = snapshot.error;
      }
    }
    assert(census !== undefined, "census should have been set");
    assert(state !== undefined, "state should have been set");
    assert(statusText !== undefined, "statusText should have been set");

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

      case diffingState.ERROR:
      case states.ERROR:
        content = [
          dom.span({ className: "snapshot-status error" }, statusText),
          dom.pre({}, safeErrorString(error))
        ];
        break;

      case diffingState.SELECTING:
      case diffingState.TAKING_DIFF:
      case states.IMPORTING:
      case states.SAVING:
      case states.SAVED:
      case states.READING:
      case states.READ:
      case states.SAVING_CENSUS:
        const throbber = state === diffingState.SELECTING
          ? ""
          : "devtools-throbber";
        content = [dom.span({ className: `snapshot-status ${throbber}` },
                            statusText)];
        break;

      case diffingState.TOOK_DIFF:
      case states.SAVED_CENSUS:
        content = [];

        if (census.breakdown.by === "allocationStack"
            && census.report.children.length === 1
            && census.report.children[0].name === "noStack") {
          content.push(dom.div({ className: "error no-allocation-stacks" },
                               L10N.getStr("heapview.noAllocationStacks")));
        }

        content.push(
          dom.div({ className: "header" },
            dom.span({ className: "heap-tree-item-bytes" }, L10N.getStr("heapview.field.bytes")),
            dom.span({ className: "heap-tree-item-count" }, L10N.getStr("heapview.field.count")),
            dom.span({ className: "heap-tree-item-total-bytes" }, L10N.getStr("heapview.field.totalbytes")),
            dom.span({ className: "heap-tree-item-total-count" }, L10N.getStr("heapview.field.totalcount")),
            dom.span({ className: "heap-tree-item-name" }, L10N.getStr("heapview.field.name"))
          ),
          Tree(createTreeProperties(census, toolbox, diffing))
        );
        break;

      default:
        assert(false, "Unexpected state: ${state}");
    }
    assert(!!content, "Should have set content in the above switch block");

    let pane = dom.div({ className: "heap-view-panel", "data-state": state },
                       ...content);

    return (
      dom.div({ id: "heap-view", "data-state": state }, pane)
    );
  }
});
