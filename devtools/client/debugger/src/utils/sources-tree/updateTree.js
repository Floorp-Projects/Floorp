/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { addToTree } from "./addToTree";
import { collapseTree } from "./collapseTree";
import {
  createDirectoryNode,
  createParentMap,
  getPathParts,
  isInvalidUrl,
} from "./utils";
import {
  getDomain,
  createTreeNodeMatcher,
  findNodeInContents,
} from "./treeOrder";

import { getDisplayURL } from "./getURL";

function getSourcesDiff(newSources, prevSources) {
  const toAdd = [];
  const toUpdate = [];

  for (const sourceId in newSources) {
    const newSource = newSources[sourceId];
    const prevSource = prevSources ? prevSources[sourceId] : null;
    if (!prevSource) {
      toAdd.push(newSource);
    } else if (prevSource.displayURL !== newSource.displayURL) {
      toUpdate.push([prevSource, newSource]);
    }
  }

  return { toAdd, toUpdate };
}

export function createTree({ debuggeeUrl, sources, threads }) {
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
}) {
  const debuggeeHost = getDomain(debuggeeUrl);
  const contexts = Object.keys(newSources);

  let shouldUpdate = !sourceTree;
  for (const context of contexts) {
    const thread = threads.find(t => t.actor === context);
    if (!thread) {
      continue;
    }

    const { toAdd, toUpdate } = getSourcesDiff(
      Object.values(newSources[context]),
      prevSources[context] ? Object.values(prevSources[context]) : null
    );

    for (const source of toAdd) {
      shouldUpdate = true;
      addToTree(uncollapsedTree, source, debuggeeHost, thread.actor);
    }

    for (const [prevSource, newSource] of toUpdate) {
      shouldUpdate = true;
      updateInTree(
        uncollapsedTree,
        prevSource,
        newSource,
        debuggeeHost,
        thread.actor
      );
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

export function updateInTree(
  tree,
  prevSource,
  newSource,
  debuggeeHost,
  thread
) {
  const newUrl = getDisplayURL(newSource, debuggeeHost);
  const prevUrl = getDisplayURL(prevSource, debuggeeHost);

  const prevEntries = findEntries(
    tree,
    prevUrl,
    prevSource,
    thread,
    debuggeeHost
  );
  if (!prevEntries) {
    return;
  }

  if (!isInvalidUrl(newUrl, newSource)) {
    const parts = getPathParts(newUrl, thread, debuggeeHost);

    if (parts.length === prevEntries.length) {
      let match = true;
      for (let i = 0; i < parts.length - 2; i++) {
        if (parts[i].path !== prevEntries[i + 1].node.path) {
          match = false;
          break;
        }
      }

      if (match) {
        const { node, index } = prevEntries.pop();
        // This is guaranteed to be a TreeSource or else findEntries would
        // not have returned anything.
        const fileNode = node.contents[index];
        fileNode.name = parts[parts.length - 1].part;
        fileNode.path = parts[parts.length - 1].path;
        fileNode.contents = newSource;
        return;
      }
    }
  }

  // Fall back to removing the current entry and inserting a new one if we
  // are unable do a straight find-replace of the name and contents.
  for (let i = prevEntries.length - 1; i >= 0; i--) {
    const { node, index } = prevEntries[i];

    // If the node has only a single child, we want to keep stepping upward
    // to find the overall value to remove.
    if (node.contents.length > 1 || (i === 0 && thread)) {
      node.contents.splice(index, 1);
      break;
    }
  }
  addToTree(tree, newSource, debuggeeHost, thread);
}

function findEntries(tree, url, source, thread, debuggeeHost) {
  const parts = getPathParts(url, thread, debuggeeHost);

  // We're searching for the directory containing the file so we pop off the
  // potential filename. This is because the tree has some logic to inject
  // special entries when filename parts either conflict with directories, and
  // so the last bit of removal needs to do a broad search to find the exact
  // target location.
  parts.pop();

  const entries = [];
  let currentNode = tree;
  for (const { part } of parts) {
    const { found: childFound, index: childIndex } = findNodeInContents(
      currentNode,
      createTreeNodeMatcher(part, true, debuggeeHost)
    );

    if (!childFound || currentNode.type !== "directory") {
      return null;
    }

    entries.push({
      node: currentNode,
      index: childIndex,
    });

    currentNode = currentNode.contents[childIndex];
  }

  // From this point, we do a depth-first search for a node containing the
  // specified source, as mentioned above.
  const found = (function search(node) {
    if (node.type !== "directory") {
      if (node.contents.id === source.id) {
        return [];
      }
      return null;
    }

    for (let i = 0; i < node.contents.length; i++) {
      const child = node.contents[i];
      const result = search(child);
      if (result) {
        result.unshift({
          node,
          index: i,
        });
        return result;
      }
    }

    return null;
  })(currentNode);

  return found ? [...entries, ...found] : null;
}
