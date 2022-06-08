/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  nodeHasChildren,
  isPathDirectory,
  isInvalidUrl,
  partIsFile,
  createSourceNode,
  createDirectoryNode,
  getPathParts,
} from "./utils";
import { createTreeNodeMatcher, findNodeInContents } from "./treeOrder";
import { getDisplayURL } from "./getURL";

function createNodeInTree(part, path, tree, index) {
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
  parts,
  subTree,
  path,
  part,
  index,
  url,
  mainThreadHost,
  source
) {
  const addedPartIsFile = partIsFile(index, parts, url);

  const { found: childFound, index: childIndex } = findNodeInContents(
    subTree,
    createTreeNodeMatcher(part, !addedPartIsFile, mainThreadHost)
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
      createTreeNodeMatcher(
        part,
        !addedPartIsFile,
        mainThreadHost,
        source,
        true
      )
    );
    return createNodeInTree(part, path, subTree, insertIndex);
  }

  // if there is no naming conflict, we can traverse into the child
  return child;
}

/*
 * walk the source tree to the final node for a given url,
 * adding new nodes along the way
 */
function traverseTree(url, tree, mainThreadHost, source, thread) {
  const parts = getPathParts(url, thread, mainThreadHost);
  return parts.reduce(
    (subTree, { part, path, mainThreadHostIfRoot }, index) =>
      findOrCreateNode(
        parts,
        subTree,
        path,
        part,
        index,
        url,
        mainThreadHostIfRoot,
        source
      ),
    tree
  );
}

/*
 * Add a source file to a directory node in the tree
 */
function addSourceToNode(node, url, source) {
  const isFile = !isPathDirectory(url.path);

  if (node.type == "source" && !isFile) {
    throw new Error(`Unexpected type "source" at: ${node.name}`);
  }

  // if we have a file, and the subtree has no elements, overwrite the
  // subtree contents with the source
  if (isFile) {
    node.type = "source";
    return source;
  }

  let { filename } = url;

  if (filename === "(index)" && url.search) {
    filename = url.search;
  } else {
    filename += url.search;
  }

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
export function addToTree(tree, source, mainThreadHost, thread) {
  const url = getDisplayURL(source, mainThreadHost);

  if (isInvalidUrl(url, source)) {
    return;
  }

  const finalNode = traverseTree(url, tree, mainThreadHost, source, thread);

  finalNode.contents = addSourceToNode(finalNode, url, source);
}
