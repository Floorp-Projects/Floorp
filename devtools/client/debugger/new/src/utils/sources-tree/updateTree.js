"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.updateTree = updateTree;

var _addToTree = require("./addToTree");

var _collapseTree = require("./collapseTree");

var _utils = require("./utils");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function newSourcesSet(newSources, prevSources) {
  const next = newSources.toSet();
  const prev = prevSources.toSet();
  return next.subtract(prev);
}

function updateTree({
  newSources,
  prevSources,
  debuggeeUrl,
  projectRoot,
  uncollapsedTree,
  sourceTree
}) {
  const newSet = newSourcesSet(newSources, prevSources);

  for (const source of newSet) {
    (0, _addToTree.addToTree)(uncollapsedTree, source, debuggeeUrl, projectRoot);
  }

  const newSourceTree = (0, _collapseTree.collapseTree)(uncollapsedTree);
  return {
    uncollapsedTree,
    sourceTree: newSourceTree,
    parentMap: (0, _utils.createParentMap)(sourceTree),
    focusedItem: null
  };
}