/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");

loader.lazyRequireGetter(
  this,
  "isShadowRoot",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "nodeFilterConstants",
  "devtools/shared/dom-node-filter-constants"
);
loader.lazyRequireGetter(
  this,
  "standardTreeWalkerFilter",
  "devtools/server/actors/inspector/utils",
  true
);

// SKIP_TO_* arguments are used with the DocumentWalker, driving the strategy to use if
// the starting node is incompatible with the filter function of the walker.
const SKIP_TO_PARENT = "SKIP_TO_PARENT";
const SKIP_TO_SIBLING = "SKIP_TO_SIBLING";

/**
 * Wrapper for inDeepTreeWalker.  Adds filtering to the traversal methods.
 * See inDeepTreeWalker for more information about the methods.
 *
 * @param {DOMNode} node
 * @param {Window} rootWin
 * @param {Object}
 *        - {Number} whatToShow
 *          See nodeFilterConstants / inIDeepTreeWalker for options.
 *        - {Function} filter
 *          A custom filter function Taking in a DOMNode and returning an Int. See
 *          WalkerActor.nodeFilter for an example.
 *        - {String} skipTo
 *          Either SKIP_TO_PARENT or SKIP_TO_SIBLING. If the provided node is not
 *          compatible with the filter function for this walker, try to find a compatible
 *          one either in the parents or in the siblings of the node.
 *        - {Boolean} showAnonymousContent
 *          Pass true to let the walker return and traverse anonymous content.
 *          When navigating host elements to which shadow DOM is attached, the light tree
 *          will be visible only to a walker with showAnonymousContent=false. The shadow
 *          tree will only be visible to a walker with showAnonymousContent=true.
 */
function DocumentWalker(
  node,
  rootWin,
  {
    whatToShow = nodeFilterConstants.SHOW_ALL,
    filter = standardTreeWalkerFilter,
    skipTo = SKIP_TO_PARENT,
    showAnonymousContent = true,
  } = {}
) {
  if (Cu.isDeadWrapper(rootWin) || !rootWin.location) {
    throw new Error("Got an invalid root window in DocumentWalker");
  }

  this.walker = Cc["@mozilla.org/inspector/deep-tree-walker;1"].createInstance(
    Ci.inIDeepTreeWalker
  );
  this.walker.showAnonymousContent = showAnonymousContent;
  this.walker.showSubDocuments = true;
  this.walker.showDocumentsAsNodes = true;
  this.walker.init(rootWin.document, whatToShow);
  this.filter = filter;

  // Make sure that the walker knows about the initial node (which could
  // be skipped due to a filter).
  this.walker.currentNode = this.getStartingNode(node, skipTo);
}

DocumentWalker.prototype = {
  get whatToShow() {
    return this.walker.whatToShow;
  },
  get currentNode() {
    return this.walker.currentNode;
  },
  set currentNode(val) {
    this.walker.currentNode = val;
  },

  parentNode() {
    if (isShadowRoot(this.currentNode)) {
      this.currentNode = this.currentNode.host;
      return this.currentNode;
    }

    const parentNode = this.currentNode.parentNode;
    // deep-tree-walker currently does not return shadowRoot elements as parentNodes.
    if (parentNode && isShadowRoot(parentNode)) {
      this.currentNode = parentNode;
      return this.currentNode;
    }
    return this.walker.parentNode();
  },

  nextNode() {
    const node = this.walker.currentNode;
    if (!node) {
      return null;
    }

    let nextNode = this.walker.nextNode();
    while (nextNode && this.isSkippedNode(nextNode)) {
      nextNode = this.walker.nextNode();
    }

    return nextNode;
  },

  firstChild() {
    const node = this.walker.currentNode;
    if (!node) {
      return null;
    }

    let firstChild = this.walker.firstChild();
    while (firstChild && this.isSkippedNode(firstChild)) {
      firstChild = this.walker.nextSibling();
    }

    return firstChild;
  },

  lastChild() {
    const node = this.walker.currentNode;
    if (!node) {
      return null;
    }

    let lastChild = this.walker.lastChild();
    while (lastChild && this.isSkippedNode(lastChild)) {
      lastChild = this.walker.previousSibling();
    }

    return lastChild;
  },

  previousSibling() {
    let node = this.walker.previousSibling();
    while (node && this.isSkippedNode(node)) {
      node = this.walker.previousSibling();
    }
    return node;
  },

  nextSibling() {
    let node = this.walker.nextSibling();
    while (node && this.isSkippedNode(node)) {
      node = this.walker.nextSibling();
    }
    return node;
  },

  getStartingNode(node, skipTo) {
    // Keep a reference on the starting node in case we can't find a node compatible with
    // the filter.
    const startingNode = node;

    if (skipTo === SKIP_TO_PARENT) {
      while (node && this.isSkippedNode(node)) {
        node = node.parentNode;
      }
    } else if (skipTo === SKIP_TO_SIBLING) {
      node = this.getClosestAcceptedSibling(node);
    }

    return node || startingNode;
  },

  /**
   * Loop on all of the provided node siblings until finding one that is compliant with
   * the filter function.
   */
  getClosestAcceptedSibling(node) {
    if (this.filter(node) === nodeFilterConstants.FILTER_ACCEPT) {
      // node is already valid, return immediately.
      return node;
    }

    // Loop on starting node siblings.
    let previous = node;
    let next = node;
    while (previous || next) {
      previous = previous?.previousSibling;
      next = next?.nextSibling;

      if (
        previous &&
        this.filter(previous) === nodeFilterConstants.FILTER_ACCEPT
      ) {
        // A valid node was found in the previous siblings of the node.
        return previous;
      }

      if (next && this.filter(next) === nodeFilterConstants.FILTER_ACCEPT) {
        // A valid node was found in the next siblings of the node.
        return next;
      }
    }

    return null;
  },

  isSkippedNode(node) {
    return this.filter(node) === nodeFilterConstants.FILTER_SKIP;
  },
};

exports.DocumentWalker = DocumentWalker;
exports.SKIP_TO_PARENT = SKIP_TO_PARENT;
exports.SKIP_TO_SIBLING = SKIP_TO_SIBLING;
