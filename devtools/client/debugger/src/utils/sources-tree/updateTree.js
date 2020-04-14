/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addToTree } from "./addToTree";
import { collapseTree } from "./collapseTree";
import { createDirectoryNode, createParentMap } from "./utils";
import { getDomain } from "./treeOrder";

import type { SourcesMapByThread } from "../../reducers/types";
import type { Thread, Source, URL } from "../../types";
import type { TreeDirectory, TreeNode } from "./types";

function getSourcesToAdd(newSources, prevSources): Source[] {
  const sourcesToAdd = [];

  for (const sourceId in newSources) {
    const newSource = newSources[sourceId];
    const prevSource = prevSources ? prevSources[sourceId] : null;
    if (!prevSource) {
      sourcesToAdd.push(newSource);
    }
  }

  return sourcesToAdd;
}

type UpdateTreeParams = {
  newSources: SourcesMapByThread,
  prevSources: SourcesMapByThread,
  uncollapsedTree: TreeDirectory,
  debuggeeUrl: URL,
  threads: Thread[],
  sourceTree?: TreeNode,
};

type CreateTreeParams = {
  sources: SourcesMapByThread,
  debuggeeUrl: URL,
  threads: Thread[],
};

export function createTree({
  debuggeeUrl,
  sources,
  threads,
}: CreateTreeParams) {
  const uncollapsedTree = createDirectoryNode("root", "", []);
  const result = updateTree({
    debuggeeUrl,
    newSources: sources,
    prevSources: {},
    threads,
    uncollapsedTree,
  });

  if (!result) {
    throw new Error("Tree must exist");
  }

  return result;
}

export function updateTree({
  newSources,
  prevSources,
  debuggeeUrl,
  uncollapsedTree,
  threads,
  create,
  sourceTree,
}: UpdateTreeParams) {
  const debuggeeHost = getDomain(debuggeeUrl);
  const contexts = (Object.keys(newSources): any);

  let shouldUpdate = !sourceTree;
  for (const context of contexts) {
    const thread = threads.find(t => t.actor === context);
    if (!thread) {
      return;
    }

    const sourcesToAdd = getSourcesToAdd(
      (Object.values(newSources[context]): any),
      prevSources[context] ? (Object.values(prevSources[context]): any) : null
    );

    for (const source of sourcesToAdd) {
      shouldUpdate = true;
      addToTree(uncollapsedTree, source, debuggeeHost, thread.actor);
    }
  }

  if (!shouldUpdate) {
    return false;
  }

  const newSourceTree = collapseTree(uncollapsedTree);

  return {
    uncollapsedTree,
    sourceTree: newSourceTree,
    parentMap: createParentMap(newSourceTree),
  };
}
