/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  nodeHasChildren,
  isPathDirectory,
  isInvalidUrl,
  partIsFile,
  createSourceNode,
  createDirectoryNode,
} from "./utils";
import { createTreeNodeMatcher, findNodeInContents } from "./treeOrder";
import { getURL } from "./getURL";

import type { ParsedURL } from "./getURL";
import type { TreeDirectory, TreeNode } from "./types";
import type { Source } from "../../types";

function createNodeInTree(
  part: string,
  path: string,
  tree: TreeDirectory,
  index: number
): TreeDirectory {
  const node = createDirectoryNode(part, path, []);

  // we are modifying the tree
  const contents = tree.contents.slice(0);
  contents.splice(index, 0, node);
  tree.contents = contents;

  return node;
}

/*
 * Look for the child node
 * 1. if it exists return it
 * 2. if it does not exist create it
 */
function findOrCreateNode(
  parts: string[],
  subTree: TreeDirectory,
  path: string,
  part: string,
  index: number,
  url: Object,
  debuggeeHost: ?string,
  source: Source
): TreeDirectory {
  const addedPartIsFile = partIsFile(index, parts, url);

  const { found: childFound, index: childIndex } = findNodeInContents(
    subTree,
    createTreeNodeMatcher(part, !addedPartIsFile, debuggeeHost)
  );

  // we create and enter the new node
  if (!childFound) {
    return createNodeInTree(part, path, subTree, childIndex);
  }

  // we found a path with the same name as the part. We need to determine
  // if this is the correct child, or if we have a naming conflict
  const child = subTree.contents[childIndex];
  const childIsFile = !nodeHasChildren(child);

  // if we have a naming conflict, we'll create a new node
  if (childIsFile != addedPartIsFile) {
    // pass true to findNodeInContents to sort node by url
    const { index: insertIndex } = findNodeInContents(
      subTree,
      createTreeNodeMatcher(part, !addedPartIsFile, debuggeeHost, source, true)
    );
    return createNodeInTree(part, path, subTree, insertIndex);
  }

  // if there is no naming conflict, we can traverse into the child
  return (child: any);
}

/*
 * walk the source tree to the final node for a given url,
 * adding new nodes along the way
 */
function traverseTree(
  url: ParsedURL,
  tree: TreeDirectory,
  debuggeeHost: ?string,
  source: Source,
  thread: string
): TreeNode {
  const parts = url.path.replace(/\/$/, "").split("/");
  parts[0] = url.group;
  if (thread) {
    parts.unshift(thread);
  }

  let path = "";
  return parts.reduce((subTree, part, index) => {
    if (index == 0 && thread) {
      path = thread;
    } else {
      path = `${path}/${part}`;
    }

    const debuggeeHostIfRoot = index === 1 ? debuggeeHost : null;

    return findOrCreateNode(
      parts,
      subTree,
      path,
      part,
      index,
      url,
      debuggeeHostIfRoot,
      source
    );
  }, tree);
}

/*
 * Add a source file to a directory node in the tree
 */
function addSourceToNode(
  node: TreeDirectory,
  url: ParsedURL,
  source: Source
): Source | TreeNode[] {
  const isFile = !isPathDirectory(url.path);

  if (node.type == "source" && !isFile) {
    throw new Error(`Unexpected type "source" at: ${node.name}`);
  }

  // if we have a file, and the subtree has no elements, overwrite the
  // subtree contents with the source
  if (isFile) {
    // $FlowIgnore
    node.type = "source";
    return source;
  }

  const { filename } = url;
  const { found: childFound, index: childIndex } = findNodeInContents(
    node,
    createTreeNodeMatcher(filename, false, null)
  );

  // if we are readding an existing file in the node, overwrite the existing
  // file and return the node's contents
  if (childFound) {
    const existingNode = node.contents[childIndex];
    if (existingNode.type === "source") {
      existingNode.contents = source;
    }

    return node.contents;
  }

  // if this is a new file, add the new file;
  const newNode = createSourceNode(filename, source.url, source);
  const contents = node.contents.slice(0);
  contents.splice(childIndex, 0, newNode);
  return contents;
}

/**
 * @memberof utils/sources-tree
 * @static
 */
export function addToTree(
  tree: TreeDirectory,
  source: Source,
  debuggeeHost: ?string,
  thread: string
): void {
  const url = getURL(source, debuggeeHost);

  if (isInvalidUrl(url, source)) {
    return;
  }

  const finalNode = traverseTree(url, tree, debuggeeHost, source, thread);

  // $FlowIgnore
  finalNode.contents = addSourceToNode(finalNode, url, source);
}
