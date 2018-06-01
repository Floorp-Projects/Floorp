/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test the HeapAnalyses{Client,Worker} "getImmediatelyDominated" request.

const breakdown = {
  by: "coarseType",
  objects: { by: "count", count: true, bytes: true },
  scripts: { by: "count", count: true, bytes: true },
  strings: { by: "count", count: true, bytes: true },
  other: { by: "count", count: true, bytes: true },
};

add_task(async function() {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  await client.readHeapSnapshot(snapshotFilePath);
  const dominatorTreeId = await client.computeDominatorTree(snapshotFilePath);

  const partialTree = await client.getDominatorTree({
    dominatorTreeId,
    breakdown
  });
  ok(partialTree.children.length > 0,
     "root should immediately dominate some nodes");

  // First, test getting a subset of children available.
  const response = await client.getImmediatelyDominated({
    dominatorTreeId,
    breakdown,
    nodeId: partialTree.nodeId,
    startIndex: 0,
    maxCount: partialTree.children.length - 1
  });

  ok(Array.isArray(response.nodes));
  ok(response.nodes.every(node => node.parentId === partialTree.nodeId));
  ok(response.moreChildrenAvailable);
  equal(response.path.length, 1);
  equal(response.path[0], partialTree.nodeId);

  for (const node of response.nodes) {
    equal(typeof node.shortestPaths, "object",
          "Should have shortest paths");
    equal(typeof node.shortestPaths.nodes, "object",
          "Should have shortest paths' nodes");
    equal(typeof node.shortestPaths.edges, "object",
          "Should have shortest paths' edges");
  }

  // Next, test getting a subset of children available.
  const secondResponse = await client.getImmediatelyDominated({
    dominatorTreeId,
    breakdown,
    nodeId: partialTree.nodeId,
    startIndex: 0,
    maxCount: Infinity
  });

  ok(Array.isArray(secondResponse.nodes));
  ok(secondResponse.nodes.every(node => node.parentId === partialTree.nodeId));
  ok(!secondResponse.moreChildrenAvailable);
  equal(secondResponse.path.length, 1);
  equal(secondResponse.path[0], partialTree.nodeId);

  for (const node of secondResponse.nodes) {
    equal(typeof node.shortestPaths, "object",
          "Should have shortest paths");
    equal(typeof node.shortestPaths.nodes, "object",
          "Should have shortest paths' nodes");
    equal(typeof node.shortestPaths.edges, "object",
          "Should have shortest paths' edges");
  }

  client.destroy();
});
