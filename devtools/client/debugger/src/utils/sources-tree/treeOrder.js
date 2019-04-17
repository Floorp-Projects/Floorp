/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { parse } from "../url";

import { nodeHasChildren } from "./utils";

import type { TreeNode } from "./types";

import type { Source } from "../../types";

/*
 * Gets domain from url (without www prefix)
 */
export function getDomain(url?: string): ?string {
  // TODO: define how files should be ordered on the browser debugger
  if (!url) {
    return null;
  }
  const { host } = parse(url);
  if (!host) {
    return null;
  }
  return host.startsWith("www.") ? host.substr("www.".length) : host;
}

/*
 * Checks if node name matches debugger host/domain.
 */
function isExactDomainMatch(part: string, debuggeeHost: string): boolean {
  return part.startsWith("www.")
    ? part.substr("www.".length) === debuggeeHost
    : part === debuggeeHost;
}

/*
 * Function to assist with node search for a defined sorted order, see e.g.
 * `createTreeNodeMatcher`. Returns negative number if the node
 * stands earlier in sorting order, positive number if the node stands later
 * in sorting order, or zero if the node is found.
 */
export type FindNodeInContentsMatcher = (node: TreeNode) => number;

/*
 * Performs a binary search to insert a node into contents. Returns positive
 * number, index of the found child, or negative number, which can be used
 * to calculate a position where a new node can be inserted (`-index - 1`).
 * The matcher is a function that returns result of comparision of a node with
 * lookup value.
 */
export function findNodeInContents(
  tree: TreeNode,
  matcher: FindNodeInContentsMatcher
) {
  if (tree.type === "source" || tree.contents.length === 0) {
    return { found: false, index: 0 };
  }

  let left = 0;
  let right = tree.contents.length - 1;
  while (left < right) {
    const middle = Math.floor((left + right) / 2);
    if (matcher(tree.contents[middle]) < 0) {
      left = middle + 1;
    } else {
      right = middle;
    }
  }
  const result = matcher(tree.contents[left]);
  if (result === 0) {
    return { found: true, index: left };
  }
  return { found: false, index: result > 0 ? left : left + 1 };
}

const IndexName = "(index)";

function createTreeNodeMatcherWithIndex(): FindNodeInContentsMatcher {
  return (node: TreeNode) => (node.name === IndexName ? 0 : 1);
}

function createTreeNodeMatcherWithDebuggeeHost(
  debuggeeHost: string
): FindNodeInContentsMatcher {
  return (node: TreeNode) => {
    if (node.name === IndexName) {
      return -1;
    }
    return isExactDomainMatch(node.name, debuggeeHost) ? 0 : 1;
  };
}

function createTreeNodeMatcherWithNameAndOther(
  part: string,
  isDir: boolean,
  debuggeeHost: ?string,
  source?: Source,
  sortByUrl?: boolean
): FindNodeInContentsMatcher {
  return (node: TreeNode) => {
    if (node.name === IndexName) {
      return -1;
    }
    if (debuggeeHost && isExactDomainMatch(node.name, debuggeeHost)) {
      return -1;
    }
    const nodeIsDir = nodeHasChildren(node);
    if (nodeIsDir && !isDir) {
      return -1;
    } else if (!nodeIsDir && isDir) {
      return 1;
    }
    if (sortByUrl && node.type === "source" && source) {
      return node.contents.url.localeCompare(source.url);
    }

    return node.name.localeCompare(part);
  };
}

/*
 * Creates a matcher for findNodeInContents.
 * The sorting order of nodes during comparison is:
 * - "(index)" node
 * - root node with the debuggee host/domain
 * - hosts/directories (not files) sorted by name
 * - files sorted by name
 */
export function createTreeNodeMatcher(
  part: string,
  isDir: boolean,
  debuggeeHost: ?string,
  source?: Source,
  sortByUrl?: boolean
): FindNodeInContentsMatcher {
  if (part === IndexName) {
    // Specialied matcher, when we are looking for "(index)" position.
    return createTreeNodeMatcherWithIndex();
  }

  if (debuggeeHost && isExactDomainMatch(part, debuggeeHost)) {
    // Specialied matcher, when we are looking for domain position.
    return createTreeNodeMatcherWithDebuggeeHost(debuggeeHost);
  }

  // Rest of the cases, without mentioned above.
  return createTreeNodeMatcherWithNameAndOther(
    part,
    isDir,
    debuggeeHost,
    source,
    sortByUrl
  );
}
