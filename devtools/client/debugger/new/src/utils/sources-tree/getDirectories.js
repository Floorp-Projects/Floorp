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
    if (subtree.type === "directory") {
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

  sourceTree.contents.forEach(node => _traverse(node));

  if (!returnTarget) {
    return sourceTree;
  }

  return returnTarget;
}

function getDirectories(source, sourceTree) {
  const url = (0, _getURL.getURL)(source);
  const fullUrl = `${url.group}${url.path}`;
  const parentMap = (0, _utils.createParentMap)(sourceTree);
  const subtreeSource = findSource(sourceTree, fullUrl);

  if (!subtreeSource) {
    return [];
  }

  let node = subtreeSource;
  const directories = [];
  directories.push(subtreeSource);

  while (true) {
    node = parentMap.get(node);

    if (!node) {
      return directories;
    }

    directories.push(node);
  }
}