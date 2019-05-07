/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { createDirectoryNode } from "./utils";

import type { TreeDirectory, TreeNode } from "./types";

/**
 * Take an existing source tree, and return a new one with collapsed nodes.
 */
function _collapseTree(node: TreeNode, depth: number): TreeNode {
  // Node is a folder.
  if (node.type === "directory") {
    if (!Array.isArray(node.contents)) {
      console.log(`Expected array at: ${node.path}`);
    }

    // Node is not a (1) thread and (2) root/domain node,
    // and only contains 1 item.
    if (depth > 2 && node.contents.length === 1) {
      const next = node.contents[0];
      // Do not collapse if the next node is a leaf node.
      if (next.type === "directory") {
        if (!Array.isArray(next.contents)) {
          console.log(
            `Expected array at: ${next.name} -- ${
              node.name
            } -- ${JSON.stringify(next.contents)}`
          );
        }
        const name = `${node.name}/${next.name}`;
        const nextNode = createDirectoryNode(name, next.path, next.contents);
        return _collapseTree(nextNode, depth + 1);
      }
    }

    // Map the contents.
    return createDirectoryNode(
      node.name,
      node.path,
      node.contents.map(next => _collapseTree(next, depth + 1))
    );
  }

  // Node is a leaf, not a folder, do not modify it.
  return node;
}

export function collapseTree(node: TreeDirectory): TreeDirectory {
  const tree = _collapseTree(node, 0);
  return ((tree: any): TreeDirectory);
}
