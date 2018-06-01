/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Compress a set of paths leading to `target` into a single graph, returned as
 * a set of nodes and a set of edges.
 *
 * @param {NodeId} target
 *        The target node passed to `HeapSnapshot.computeShortestPaths`.
 *
 * @param {Array<Path>} paths
 *        An array of paths to `target`, as returned by
 *        `HeapSnapshot.computeShortestPaths`.
 *
 * @returns {Object}
 *          An object with two properties:
 *          - edges: An array of unique objects of the form:
 *              {
 *                 from: <node ID>,
 *                 to: <node ID>,
 *                 name: <string or null>
 *              }
 *          - nodes: An array of unique node IDs. Every `from` and `to` id is
 *            guaranteed to be in this array exactly once.
 */
exports.deduplicatePaths = function(target, paths) {
  // Use this structure to de-duplicate edges among many retaining paths from
  // start to target.
  //
  // Map<FromNodeId, Map<ToNodeId, Set<EdgeName>>>
  const deduped = new Map();

  function insert(from, to, name) {
    let toMap = deduped.get(from);
    if (!toMap) {
      toMap = new Map();
      deduped.set(from, toMap);
    }

    let nameSet = toMap.get(to);
    if (!nameSet) {
      nameSet = new Set();
      toMap.set(to, nameSet);
    }

    nameSet.add(name);
  }

  // eslint-disable-next-line no-labels
  outer: for (const path of paths) {
    const pathLength = path.length;

    // Check for duplicate predecessors in the path, and skip paths that contain
    // them.
    const predecessorsSeen = new Set();
    predecessorsSeen.add(target);
    for (let i = 0; i < pathLength; i++) {
      if (predecessorsSeen.has(path[i].predecessor)) {
        // eslint-disable-next-line no-labels
        continue outer;
      }
      predecessorsSeen.add(path[i].predecessor);
    }

    for (let i = 0; i < pathLength - 1; i++) {
      insert(path[i].predecessor, path[i + 1].predecessor, path[i].edge);
    }

    insert(path[pathLength - 1].predecessor, target, path[pathLength - 1].edge);
  }

  const nodes = [target];
  const edges = [];

  for (const [from, toMap] of deduped) {
    // If the second/third/etc shortest path contains the `target` anywhere
    // other than the very last node, we could accidentally put the `target` in
    // `nodes` more than once.
    if (from !== target) {
      nodes.push(from);
    }

    for (const [to, edgeNameSet] of toMap) {
      for (const name of edgeNameSet) {
        edges.push({ from, to, name });
      }
    }
  }

  return { nodes, edges };
};
