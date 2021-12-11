/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createParentMap } from "./utils";
import flattenDeep from "lodash/flattenDeep";

function findSourceItem(sourceTree, source) {
  function _traverse(subtree) {
    if (subtree.type === "source") {
      if (subtree.contents.id === source.id) {
        return subtree;
      }

      return null;
    }

    const matches = subtree.contents.map(child => _traverse(child));
    return matches && matches.filter(Boolean)[0];
  }

  return _traverse(sourceTree);
}

export function findSourceTreeNodes(sourceTree, path) {
  function _traverse(subtree) {
    if (subtree.path.endsWith(path)) {
      return subtree;
    }

    if (subtree.type === "directory") {
      const matches = subtree.contents.map(child => _traverse(child));
      return matches && matches.filter(Boolean);
    }
  }

  const result = _traverse(sourceTree);
  return Array.isArray(result) ? flattenDeep(result) : result;
}

function getAncestors(sourceTree, item) {
  if (!item) {
    return null;
  }

  const parentMap = createParentMap(sourceTree);
  const directories = [];

  directories.push(item);
  while (true) {
    item = parentMap.get(item);
    if (!item) {
      return directories;
    }
    directories.push(item);
  }
}

export function getDirectories(source, sourceTree) {
  const item = findSourceItem(sourceTree, source);
  const ancestors = getAncestors(sourceTree, item);
  return ancestors || [sourceTree];
}
