/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addToTree } from "./addToTree";
import { collapseTree } from "./collapseTree";
import { createParentMap, isSource, partIsFile } from "./utils";
import { difference } from "lodash";
import {
  getDomain,
  findNodeInContents,
  createTreeNodeMatcher
} from "./treeOrder";
import type { SourcesMap } from "../../reducers/types";
import type { TreeDirectory, TreeNode } from "./types";

function newSourcesSet(newSources, prevSources) {
  const newSourceIds = difference(
    Object.keys(newSources),
    Object.keys(prevSources)
  );
  const uniqSources = newSourceIds.map(id => newSources[id]);
  return uniqSources;
}

function findFocusedItemInTree(
  newSourceTree: TreeDirectory,
  focusedItem: TreeNode,
  debuggeeHost: ?string
): ?TreeNode {
  const parts = focusedItem.path.split("/").filter(p => p !== "");
  let path = "";

  return parts.reduce((subTree, part, index) => {
    if (subTree === undefined || subTree === null) {
      return null;
    } else if (isSource(subTree)) {
      return subTree;
    }

    path = path ? `${path}/${part}` : part;
    const { index: childIndex } = findNodeInContents(
      subTree,
      createTreeNodeMatcher(
        part,
        !partIsFile(index, parts, focusedItem),
        debuggeeHost
      )
    );

    return subTree.contents[childIndex];
  }, newSourceTree);
}

type Params = {
  newSources: SourcesMap,
  prevSources: SourcesMap,
  uncollapsedTree: TreeDirectory,
  sourceTree: TreeDirectory,
  debuggeeUrl: string,
  projectRoot: string,
  focusedItem: ?TreeNode
};

export function updateTree({
  newSources,
  prevSources,
  debuggeeUrl,
  projectRoot,
  uncollapsedTree,
  sourceTree,
  focusedItem
}: Params) {
  const newSet = newSourcesSet(newSources, prevSources);
  const debuggeeHost = getDomain(debuggeeUrl);

  for (const source of newSet) {
    addToTree(uncollapsedTree, source, debuggeeHost, projectRoot);
  }

  const newSourceTree = collapseTree(uncollapsedTree);

  if (focusedItem) {
    focusedItem = findFocusedItemInTree(
      newSourceTree,
      focusedItem,
      debuggeeHost
    );
  }

  return {
    uncollapsedTree,
    sourceTree: newSourceTree,
    parentMap: createParentMap(newSourceTree),
    focusedItem
  };
}
