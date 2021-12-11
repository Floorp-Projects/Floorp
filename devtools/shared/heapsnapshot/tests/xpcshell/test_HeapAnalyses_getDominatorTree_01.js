/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test the HeapAnalyses{Client,Worker} "getDominatorTree" request.

const breakdown = {
  by: "coarseType",
  objects: { by: "count", count: true, bytes: true },
  scripts: { by: "count", count: true, bytes: true },
  strings: { by: "count", count: true, bytes: true },
  other: { by: "count", count: true, bytes: true },
  domNode: { by: "count", count: true, bytes: true },
};

add_task(async function() {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  await client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const dominatorTreeId = await client.computeDominatorTree(snapshotFilePath);
  equal(
    typeof dominatorTreeId,
    "number",
    "should get a dominator tree id, and it should be a number"
  );

  const partialTree = await client.getDominatorTree({
    dominatorTreeId,
    breakdown,
  });
  ok(partialTree, "Should get a partial tree");
  equal(typeof partialTree, "object", "partialTree should be an object");

  function checkTree(node) {
    equal(typeof node.nodeId, "number", "each node should have an id");

    if (node === partialTree) {
      equal(node.parentId, undefined, "the root has no parent");
    } else {
      equal(
        typeof node.parentId,
        "number",
        "each node should have a parent id"
      );
    }

    equal(
      typeof node.retainedSize,
      "number",
      "each node should have a retained size"
    );

    ok(
      node.children === undefined || Array.isArray(node.children),
      "each node either has a list of children, " +
        "or undefined meaning no children loaded"
    );
    equal(
      typeof node.moreChildrenAvailable,
      "boolean",
      "each node should indicate if there are more children available or not"
    );

    equal(typeof node.shortestPaths, "object", "Should have shortest paths");
    equal(
      typeof node.shortestPaths.nodes,
      "object",
      "Should have shortest paths' nodes"
    );
    equal(
      typeof node.shortestPaths.edges,
      "object",
      "Should have shortest paths' edges"
    );

    if (node.children) {
      node.children.forEach(checkTree);
    }
  }

  checkTree(partialTree);

  client.destroy();
});
