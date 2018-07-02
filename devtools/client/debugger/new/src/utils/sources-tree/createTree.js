"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createTree = createTree;

var _addToTree = require("./addToTree");

var _collapseTree = require("./collapseTree");

var _utils = require("./utils");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function createTree({
  sources,
  debuggeeUrl,
  projectRoot
}) {
  const uncollapsedTree = (0, _utils.createDirectoryNode)("root", "", []);

  for (const sourceId in sources) {
    const source = sources[sourceId];
    (0, _addToTree.addToTree)(uncollapsedTree, source, debuggeeUrl, projectRoot);
  }

  const sourceTree = (0, _collapseTree.collapseTree)(uncollapsedTree);
  return {
    uncollapsedTree,
    sourceTree,
    parentMap: (0, _utils.createParentMap)(sourceTree),
    focusedItem: null
  };
}