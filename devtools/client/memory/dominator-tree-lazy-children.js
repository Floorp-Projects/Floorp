/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The `DominatorTreeLazyChildren` is a placeholder that represents a future
 * subtree in an existing `DominatorTreeNode` tree that is currently being
 * incrementally fetched from the `HeapAnalysesWorker`.
 *
 * @param {NodeId} parentNodeId
 * @param {Number} siblingIndex
 */
function DominatorTreeLazyChildren(parentNodeId, siblingIndex) {
  this._parentNodeId = parentNodeId;
  this._siblingIndex = siblingIndex;
}

/**
 * Generate a unique key for this `DominatorTreeLazyChildren` instance. This can
 * be used as the key in a hash table or as the `key` property for a React
 * component, for example.
 *
 * @returns {String}
 */
DominatorTreeLazyChildren.prototype.key = function () {
  return `dominator-tree-lazy-children-${this._parentNodeId}-${this._siblingIndex}`;
};

/**
 * Return true if this is a placeholder for the first child of its
 * parent. Return false if it is a placeholder for loading more of its parent's
 * children.
 *
 * @returns {Boolean}
 */
DominatorTreeLazyChildren.prototype.isFirstChild = function () {
  return this._siblingIndex === 0;
};

/**
 * Get this subtree's parent node's identifier.
 *
 * @returns {NodeId}
 */
DominatorTreeLazyChildren.prototype.parentNodeId = function () {
  return this._parentNodeId;
};

/**
 * Get this subtree's index in its parent's children array.
 *
 * @returns {Number}
 */
DominatorTreeLazyChildren.prototype.siblingIndex = function () {
  return this._siblingIndex;
};

module.exports = DominatorTreeLazyChildren;
