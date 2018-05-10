"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getDirectories = getDirectories;

var _utils = require("./utils");

var _getURL = require("./getURL");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function findSource(sourceTree, sourceUrl) {
  let returnTarget = null;

  function _traverse(subtree) {
    if ((0, _utils.nodeHasChildren)(subtree)) {
      for (const child of subtree.contents) {
        _traverse(child);
      }
    } else if (!returnTarget) {
      if (subtree.path.replace(/http(s)?:\//, "") == sourceUrl) {
        returnTarget = subtree;
        return;
      }
    }
  }

  sourceTree.contents.forEach(_traverse);

  if (!returnTarget) {
    return sourceTree;
  }

  return returnTarget;
}

function getDirectories(sourceUrl, sourceTree) {
  const url = (0, _getURL.getURL)(sourceUrl);
  const fullUrl = `${url.group}${url.path}`;
  const parentMap = (0, _utils.createParentMap)(sourceTree);
  const source = findSource(sourceTree, fullUrl);

  if (!source) {
    return [];
  }

  let node = source;
  const directories = [];
  directories.push(source);

  while (true) {
    node = parentMap.get(node);

    if (!node) {
      return directories;
    }

    directories.push(node);
  }
}