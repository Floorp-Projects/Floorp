/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the HeapAnalyses{Client,Worker} "getImmediatelyDominated" request.

function run_test() {
  run_next_test();
}

add_task(function* () {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  yield client.readHeapSnapshot(snapshotFilePath);
  const dominatorTreeId = yield client.computeDominatorTree(snapshotFilePath);

  const partialTree = yield client.getDominatorTree({ dominatorTreeId });
  ok(partialTree.children.length > 0,
     "root should immediately dominate some nodes");

  // First, test getting a subset of children available.
  const response = yield client.getImmediatelyDominated({
    dominatorTreeId,
    nodeId: partialTree.nodeId,
    startIndex: 0,
    maxCount: partialTree.children.length - 1
  });

  ok(Array.isArray(response.nodes));
  ok(response.nodes.every(node => node.parentId === partialTree.nodeId));
  ok(response.moreChildrenAvailable);

  // Next, test getting a subset of children available.
  const secondResponse = yield client.getImmediatelyDominated({
    dominatorTreeId,
    nodeId: partialTree.nodeId,
    startIndex: 0,
    maxCount: Infinity
  });

  ok(Array.isArray(secondResponse.nodes));
  ok(secondResponse.nodes.every(node => node.parentId === partialTree.nodeId));
  ok(!secondResponse.moreChildrenAvailable);

  client.destroy();
});
