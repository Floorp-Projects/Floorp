"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getDomain = getDomain;
exports.findNodeInContents = findNodeInContents;
exports.createTreeNodeMatcher = createTreeNodeMatcher;

var _url = require("devtools/client/debugger/new/dist/vendors").vendored["url"];

var _utils = require("./utils");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Gets domain from url (without www prefix)
 */
function getDomain(url) {
  // TODO: define how files should be ordered on the browser debugger
  if (!url) {
    return null;
  }

  const {
    host
  } = (0, _url.parse)(url);

  if (!host) {
    return null;
  }

  return host.startsWith("www.") ? host.substr("www.".length) : host;
}
/*
 * Checks if node name matches debugger host/domain.
 */


function isExactDomainMatch(part, debuggeeHost) {
  return part.startsWith("www.") ? part.substr("www.".length) === debuggeeHost : part === debuggeeHost;
}
/*
 * Function to assist with node search for a defined sorted order, see e.g.
 * `createTreeNodeMatcher`. Returns negative number if the node
 * stands earlier in sorting order, positive number if the node stands later
 * in sorting order, or zero if the node is found.
 */


/*
 * Performs a binary search to insert a node into contents. Returns positive
 * number, index of the found child, or negative number, which can be used
 * to calculate a position where a new node can be inserted (`-index - 1`).
 * The matcher is a function that returns result of comparision of a node with
 * lookup value.
 */
function findNodeInContents(tree, matcher) {
  if (tree.type === "source" || tree.contents.length === 0) {
    return {
      found: false,
      index: 0
    };
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
    return {
      found: true,
      index: left
    };
  }

  return {
    found: false,
    index: result > 0 ? left : left + 1
  };
}

const IndexName = "(index)";

function createTreeNodeMatcherWithIndex() {
  return node => node.name === IndexName ? 0 : 1;
}

function createTreeNodeMatcherWithDebuggeeHost(debuggeeHost) {
  return node => {
    if (node.name === IndexName) {
      return -1;
    }

    return isExactDomainMatch(node.name, debuggeeHost) ? 0 : 1;
  };
}

function createTreeNodeMatcherWithNameAndOther(part, isDir, debuggeeHost) {
  return node => {
    if (node.name === IndexName) {
      return -1;
    }

    if (debuggeeHost && isExactDomainMatch(node.name, debuggeeHost)) {
      return -1;
    }

    const nodeIsDir = (0, _utils.nodeHasChildren)(node);

    if (nodeIsDir && !isDir) {
      return -1;
    } else if (!nodeIsDir && isDir) {
      return 1;
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


function createTreeNodeMatcher(part, isDir, debuggeeHost) {
  if (part === IndexName) {
    // Specialied matcher, when we are looking for "(index)" position.
    return createTreeNodeMatcherWithIndex();
  }

  if (debuggeeHost && isExactDomainMatch(part, debuggeeHost)) {
    // Specialied matcher, when we are looking for domain position.
    return createTreeNodeMatcherWithDebuggeeHost(debuggeeHost);
  } // Rest of the cases, without mentioned above.


  return createTreeNodeMatcherWithNameAndOther(part, isDir, debuggeeHost);
}