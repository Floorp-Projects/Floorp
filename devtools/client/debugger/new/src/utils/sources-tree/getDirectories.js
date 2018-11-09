/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { createParentMap } from "./utils";
import type { TreeNode, TreeDirectory } from "./types";
import type { Source } from "../../types";

function _traverse(subtree: TreeNode, source: Source) {
  if (subtree.type === "source") {
    if (subtree.contents.id === source.id) {
      return subtree;
    }

    return null;
  }

  const matches = subtree.contents.map(child => _traverse(child, source));
  return matches && matches.filter(Boolean)[0];
}

function findSourceItem(sourceTree: TreeDirectory, source: Source): ?TreeNode {
  return _traverse(sourceTree, source);
}

function getAncestors(sourceTree: TreeDirectory, item: ?TreeNode) {
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

export function getDirectories(source: Source, sourceTree: TreeDirectory) {
  const item = findSourceItem(sourceTree, source);
  const ancestors = getAncestors(sourceTree, item);
  return ancestors || [sourceTree];
}
