"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.collapseTree = collapseTree;

var _utils = require("./utils");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Take an existing source tree, and return a new one with collapsed nodes.
 */
function _collapseTree(node, depth) {
  // Node is a folder.
  if (node.type === "directory") {
    if (!Array.isArray(node.contents)) {
      console.log(`WTF: ${node.path}`);
    } // Node is not a root/domain node, and only contains 1 item.


    if (depth > 1 && node.contents.length === 1) {
      const next = node.contents[0]; // Do not collapse if the next node is a leaf node.

      if (next.type === "directory") {
        if (!Array.isArray(next.contents)) {
          console.log(`WTF: ${next.name} -- ${node.name} -- ${JSON.stringify(next.contents)}`);
        }

        const name = `${node.name}/${next.name}`;
        const nextNode = (0, _utils.createDirectoryNode)(name, next.path, next.contents);
        return _collapseTree(nextNode, depth + 1);
      }
    } // Map the contents.


    return (0, _utils.createDirectoryNode)(node.name, node.path, node.contents.map(next => _collapseTree(next, depth + 1)));
  } // Node is a leaf, not a folder, do not modify it.


  return node;
}

function collapseTree(node) {
  const tree = _collapseTree(node, 0);

  return tree;
}