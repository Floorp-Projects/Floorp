/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const DEFAULT_MAX_DEPTH = 3;
const DEFAULT_MAX_SIBLINGS = 15;

/**
 * A single node in a dominator tree.
 *
 * @param {NodeId} nodeId
 * @param {NodeSize} retainedSize
 */
function DominatorTreeNode(nodeId, retainedSize) {
  // The id of this node.
  this.nodeId = nodeId;

  // The retained size of this node.
  this.retainedSize = retainedSize;

  // The id of this node's parent or undefined if this node is the root.
  this.parentId = undefined;

  // An array of immediately dominated child `DominatorTreeNode`s, or undefined.
  this.children = undefined;

  // True iff the `children` property does not contain every immediately
  // dominated node.
  //
  // * If children is an array and this property is true: the array does not
  //   contain the complete set of immediately dominated children.
  // * If children is an array and this property is false: the array contains
  //   the complete set of immediately dominated children.
  // * If children is undefined and this property is true: there exist
  //   immediately dominated children for this node, but they have not been
  //   loaded yet.
  // * If children is undefined and this property is false: this node does not
  //   dominate any others and therefore has no children.
  this.moreChildrenAvailable = true;
}

DominatorTreeNode.prototype = null;

module.exports = DominatorTreeNode;

/**
 * Add `child` to the `parent`'s set of children.
 *
 * @param {DominatorTreeNode} parent
 * @param {DominatorTreeNode} child
 */
DominatorTreeNode.addChild = function (parent, child) {
  if (parent.children === undefined) {
    parent.children = [];
  }

  parent.children.push(child);
  child.parentId = parent.nodeId;
};

/**
 * Do a partial traversal of the given dominator tree and convert it into a tree
 * of `DominatorTreeNode`s. Dominator trees have a node for every node in the
 * snapshot's heap graph, so we must not allocate a JS object for every node. It
 * would be way too many and the node count is effectively unbounded.
 *
 * Go no deeper down the tree than `maxDepth` and only consider at most
 * `maxSiblings` within any single node's children.
 *
 * @param {DominatorTree} dominatorTree
 * @param {Number} maxDepth
 * @param {Number} maxSiblings
 *
 * @returns {DominatorTreeNode}
 */
DominatorTreeNode.partialTraversal = function (dominatorTree,
                                               maxDepth = DEFAULT_MAX_DEPTH,
                                               maxSiblings = DEFAULT_MAX_SIBLINGS) {
  function dfs(nodeId, depth) {
    const size = dominatorTree.getRetainedSize(nodeId);
    const node = new DominatorTreeNode(nodeId, size);
    const childNodeIds = dominatorTree.getImmediatelyDominated(nodeId);

    const newDepth = depth + 1;
    if (newDepth < maxDepth) {
      const endIdx = Math.min(childNodeIds.length, maxSiblings);
      for (let i = 0; i < endIdx; i++) {
        DominatorTreeNode.addChild(node, dfs(childNodeIds[i], newDepth));
      }
      node.moreChildrenAvailable = childNodeIds.length < endIdx;
    } else {
      node.moreChildrenAvailable = childNodeIds.length > 0;
    }

    return node;
  }

  return dfs(dominatorTree.root, 0);
};
