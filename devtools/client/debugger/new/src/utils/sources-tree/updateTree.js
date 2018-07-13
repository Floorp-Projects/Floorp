"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.updateTree = updateTree;

var _addToTree = require("./addToTree");

var _collapseTree = require("./collapseTree");

var _utils = require("./utils");

var _lodash = require("devtools/client/shared/vendor/lodash");

var _treeOrder = require("./treeOrder");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function newSourcesSet(newSources, prevSources) {
  const newSourceIds = (0, _lodash.difference)(Object.keys(newSources), Object.keys(prevSources));
  const uniqSources = newSourceIds.map(id => newSources[id]);
  return uniqSources;
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
  const debuggeeHost = (0, _treeOrder.getDomain)(debuggeeUrl);

  for (const source of newSet) {
    (0, _addToTree.addToTree)(uncollapsedTree, source, debuggeeHost, projectRoot);
  }

  const newSourceTree = (0, _collapseTree.collapseTree)(uncollapsedTree);
  return {
    uncollapsedTree,
    sourceTree: newSourceTree,
    parentMap: (0, _utils.createParentMap)(sourceTree),
    focusedItem: null
  };
}