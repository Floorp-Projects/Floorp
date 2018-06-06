"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.addToTree = addToTree;

var _utils = require("./utils");

var _treeOrder = require("./treeOrder");

var _getURL = require("./getURL");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function createNodeInTree(part, path, tree, index) {
  const node = (0, _utils.createNode)(part, path, []); // we are modifying the tree

  const contents = tree.contents.slice(0);
  contents.splice(index, 0, node);
  tree.contents = contents;
  return node;
}
/*
 * Look for the child directory
 * 1. if it exists return it
 * 2. if it does not exist create it
 * 3. if it is a file, replace it with a directory
 */


function findOrCreateNode(parts, subTree, path, part, index, url, debuggeeHost) {
  const addedPartIsFile = (0, _utils.partIsFile)(index, parts, url);
  const {
    found: childFound,
    index: childIndex
  } = (0, _treeOrder.findNodeInContents)(subTree, (0, _treeOrder.createTreeNodeMatcher)(part, !addedPartIsFile, debuggeeHost)); // we create and enter the new node

  if (!childFound) {
    return createNodeInTree(part, path, subTree, childIndex);
  } // we found a path with the same name as the part. We need to determine
  // if this is the correct child, or if we have a naming conflict


  const child = subTree.contents[childIndex];
  const childIsFile = !(0, _utils.nodeHasChildren)(child); // if we have a naming conflict, we'll create a new node

  if (childIsFile && !addedPartIsFile || !childIsFile && addedPartIsFile) {
    return createNodeInTree(part, path, subTree, childIndex);
  } // if there is no naming conflict, we can traverse into the child


  return child;
}
/*
 * walk the source tree to the final node for a given url,
 * adding new nodes along the way
 */


function traverseTree(url, tree, debuggeeHost) {
  url.path = decodeURIComponent(url.path);
  const parts = url.path.split("/").filter(p => p !== "");
  parts.unshift(url.group);
  let path = "";
  return parts.reduce((subTree, part, index) => {
    path = path ? `${path}/${part}` : part;
    const debuggeeHostIfRoot = index === 0 ? debuggeeHost : null;
    return findOrCreateNode(parts, subTree, path, part, index, url, debuggeeHostIfRoot);
  }, tree);
}
/*
 * Add a source file to a directory node in the tree
 */


function addSourceToNode(node, url, source) {
  const isFile = !(0, _utils.isDirectory)(url); // if we have a file, and the subtree has no elements, overwrite the
  // subtree contents with the source

  if (isFile) {
    return source;
  }

  const {
    filename
  } = url;
  const {
    found: childFound,
    index: childIndex
  } = (0, _treeOrder.findNodeInContents)(node, (0, _treeOrder.createTreeNodeMatcher)(filename, false, null)); // if we are readding an existing file in the node, overwrite the existing
  // file and return the node's contents

  if (childFound) {
    const existingNode = node.contents[childIndex];
    existingNode.contents = source;
    return node.contents;
  } // if this is a new file, add the new file;


  const newNode = (0, _utils.createNode)(filename, source.get("url"), source);
  const contents = node.contents.slice(0);
  contents.splice(childIndex, 0, newNode);
  return contents;
}
/**
 * @memberof utils/sources-tree
 * @static
 */


function addToTree(tree, source, debuggeeUrl, projectRoot) {
  const url = (0, _getURL.getURL)(source.url, debuggeeUrl);
  const debuggeeHost = (0, _treeOrder.getDomain)(debuggeeUrl);

  if ((0, _utils.isInvalidUrl)(url, source)) {
    return;
  }

  const finalNode = traverseTree(url, tree, debuggeeHost);
  finalNode.contents = addSourceToNode(finalNode, url, source);
}