"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.sortEntireTree = sortEntireTree;
exports.sortTree = sortTree;

var _utils = require("./utils");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Look at the nodes in the source tree, and determine the index of where to
 * insert a new node. The ordering is index -> folder -> file.
 * @memberof utils/sources-tree
 * @static
 */
function sortEntireTree(tree, debuggeeUrl = "") {
  if ((0, _utils.nodeHasChildren)(tree)) {
    const contents = sortTree(tree, debuggeeUrl).map(subtree => sortEntireTree(subtree));
    return { ...tree,
      contents
    };
  }

  return tree;
}
/**
 * Look at the nodes in the source tree, and determine the index of where to
 * insert a new node. The ordering is index -> folder -> file.
 * @memberof utils/sources-tree
 * @static
 */


function sortTree(tree, debuggeeUrl = "") {
  return tree.contents.sort((previousNode, currentNode) => {
    const currentNodeIsDir = (0, _utils.nodeHasChildren)(currentNode);
    const previousNodeIsDir = (0, _utils.nodeHasChildren)(previousNode);

    if (currentNode.name === "(index)") {
      return 1;
    } else if (previousNode.name === "(index)") {
      return -1;
    } else if ((0, _utils.isExactUrlMatch)(currentNode.name, debuggeeUrl)) {
      return 1;
    } else if ((0, _utils.isExactUrlMatch)(previousNode.name, debuggeeUrl)) {
      return -1; // If neither is the case, continue to compare alphabetically
    } else if (previousNodeIsDir && !currentNodeIsDir) {
      return -1;
    } else if (!previousNodeIsDir && currentNodeIsDir) {
      return 1;
    }

    return previousNode.name.localeCompare(currentNode.name);
  });
}