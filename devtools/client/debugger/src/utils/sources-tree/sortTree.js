/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { nodeHasChildren, isExactUrlMatch } from "./utils";

import type { TreeDirectory } from "./types";
import type { URL } from "../../types";

/**
 * Look at the nodes in the source tree, and determine the index of where to
 * insert a new node. The ordering is index -> folder -> file.
 * @memberof utils/sources-tree
 * @static
 */
export function sortTree(tree: TreeDirectory, debuggeeUrl: URL = ""): number {
  return (tree.contents: any).sort((previousNode, currentNode) => {
    const currentNodeIsDir = nodeHasChildren(currentNode);
    const previousNodeIsDir = nodeHasChildren(previousNode);
    if (currentNode.name === "(index)") {
      return 1;
    } else if (previousNode.name === "(index)") {
      return -1;
    } else if (isExactUrlMatch(currentNode.name, debuggeeUrl)) {
      return 1;
    } else if (isExactUrlMatch(previousNode.name, debuggeeUrl)) {
      return -1;
      // If neither is the case, continue to compare alphabetically
    } else if (previousNodeIsDir && !currentNodeIsDir) {
      return -1;
    } else if (!previousNodeIsDir && currentNodeIsDir) {
      return 1;
    }
    return previousNode.name.localeCompare(currentNode.name);
  });
}
