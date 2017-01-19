/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, PropTypes, createFactory } = require("devtools/client/shared/vendor/react");
const { assert } = require("devtools/shared/DevToolsUtils");
const { createParentMap } = require("devtools/shared/heapsnapshot/CensusUtils");
const Tree = createFactory(require("devtools/client/shared/components/tree"));
const DominatorTreeItem = createFactory(require("./dominator-tree-item"));
const { L10N } = require("../utils");
const { TREE_ROW_HEIGHT, dominatorTreeState } = require("../constants");
const { dominatorTreeModel } = require("../models");
const DominatorTreeLazyChildren = require("../dominator-tree-lazy-children");

const DOMINATOR_TREE_AUTO_EXPAND_DEPTH = 3;

/**
 * A throbber that represents a subtree in the dominator tree that is actively
 * being incrementally loaded and fetched from the `HeapAnalysesWorker`.
 */
const DominatorTreeSubtreeFetching = createFactory(createClass({
  displayName: "DominatorTreeSubtreeFetching",

  propTypes: {
    depth: PropTypes.number.isRequired,
    focused: PropTypes.bool.isRequired,
  },

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.depth !== nextProps.depth
      || this.props.focused !== nextProps.focused;
  },

  render() {
    let {
      depth,
      focused,
    } = this.props;

    return dom.div(
      {
        className: `heap-tree-item subtree-fetching ${focused ? "focused" : ""}`
      },
      dom.span({ className: "heap-tree-item-field heap-tree-item-bytes" }),
      dom.span({ className: "heap-tree-item-field heap-tree-item-bytes" }),
      dom.span({
        className: "heap-tree-item-field heap-tree-item-name devtools-throbber",
        style: { marginInlineStart: depth * TREE_ROW_HEIGHT }
      })
    );
  }
}));

/**
 * A link to fetch and load more siblings in the dominator tree, when there are
 * already many loaded above.
 */
const DominatorTreeSiblingLink = createFactory(createClass({
  displayName: "DominatorTreeSiblingLink",

  propTypes: {
    depth: PropTypes.number.isRequired,
    focused: PropTypes.bool.isRequired,
    item: PropTypes.instanceOf(DominatorTreeLazyChildren).isRequired,
    onLoadMoreSiblings: PropTypes.func.isRequired,
  },

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.depth !== nextProps.depth
      || this.props.focused !== nextProps.focused;
  },

  render() {
    let {
      depth,
      focused,
      item,
      onLoadMoreSiblings,
    } = this.props;

    return dom.div(
      {
        className: `heap-tree-item more-children ${focused ? "focused" : ""}`
      },
      dom.span({ className: "heap-tree-item-field heap-tree-item-bytes" }),
      dom.span({ className: "heap-tree-item-field heap-tree-item-bytes" }),
      dom.span(
        {
          className: "heap-tree-item-field heap-tree-item-name",
          style: { marginInlineStart: depth * TREE_ROW_HEIGHT }
        },
        dom.a(
          {
            onClick: () => onLoadMoreSiblings(item)
          },
          L10N.getStr("tree-item.load-more")
        )
      )
    );
  }
}));

/**
 * The actual dominator tree rendered as an expandable and collapsible tree.
 */
module.exports = createClass({
  displayName: "DominatorTree",

  propTypes: {
    dominatorTree: dominatorTreeModel.isRequired,
    onLoadMoreSiblings: PropTypes.func.isRequired,
    onViewSourceInDebugger: PropTypes.func.isRequired,
    onExpand: PropTypes.func.isRequired,
    onCollapse: PropTypes.func.isRequired,
    onFocus: PropTypes.func.isRequired,
  },

  shouldComponentUpdate(nextProps, nextState) {
    // Safe to use referential equality here because all of our mutations on
    // dominator tree models use immutableUpdate in a persistent manner. The
    // exception to the rule are mutations of the expanded set, however we take
    // care that the dominatorTree model itself is still re-allocated when
    // mutations to the expanded set occur. Because of the re-allocations, we
    // can continue using referential equality here.
    return this.props.dominatorTree !== nextProps.dominatorTree;
  },

  render() {
    const { dominatorTree, onViewSourceInDebugger, onLoadMoreSiblings } = this.props;

    const parentMap = createParentMap(dominatorTree.root, node => node.nodeId);

    return Tree({
      key: "dominator-tree-tree",
      autoExpandDepth: DOMINATOR_TREE_AUTO_EXPAND_DEPTH,
      focused: dominatorTree.focused,
      getParent: node =>
        node instanceof DominatorTreeLazyChildren
          ? parentMap[node.parentNodeId()]
          : parentMap[node.nodeId],
      getChildren: node => {
        const children = node.children ? node.children.slice() : [];
        if (node.moreChildrenAvailable) {
          children.push(new DominatorTreeLazyChildren(node.nodeId, children.length));
        }
        return children;
      },
      isExpanded: node => {
        return node instanceof DominatorTreeLazyChildren
          ? false
          : dominatorTree.expanded.has(node.nodeId);
      },
      onExpand: item => {
        if (item instanceof DominatorTreeLazyChildren) {
          return;
        }

        if (item.moreChildrenAvailable && (!item.children || !item.children.length)) {
          const startIndex = item.children ? item.children.length : 0;
          onLoadMoreSiblings(new DominatorTreeLazyChildren(item.nodeId, startIndex));
        }

        this.props.onExpand(item);
      },
      onCollapse: item => {
        if (item instanceof DominatorTreeLazyChildren) {
          return;
        }

        this.props.onCollapse(item);
      },
      onFocus: item => {
        if (item instanceof DominatorTreeLazyChildren) {
          return;
        }

        this.props.onFocus(item);
      },
      renderItem: (item, depth, focused, arrow, expanded) => {
        if (item instanceof DominatorTreeLazyChildren) {
          if (item.isFirstChild()) {
            assert(dominatorTree.state === dominatorTreeState.INCREMENTAL_FETCHING,
                   "If we are displaying a throbber for loading a subtree, " +
                   "then we should be INCREMENTAL_FETCHING those children right now");
            return DominatorTreeSubtreeFetching({
              key: item.key(),
              depth,
              focused,
            });
          }

          return DominatorTreeSiblingLink({
            key: item.key(),
            item,
            depth,
            focused,
            onLoadMoreSiblings,
          });
        }

        return DominatorTreeItem({
          item,
          depth,
          focused,
          arrow,
          expanded,
          getPercentSize: size => (size / dominatorTree.root.retainedSize) * 100,
          onViewSourceInDebugger,
        });
      },
      getRoots: () => [dominatorTree.root],
      getKey: node =>
        node instanceof DominatorTreeLazyChildren ? node.key() : node.nodeId,
      itemHeight: TREE_ROW_HEIGHT,
    });
  }
});
