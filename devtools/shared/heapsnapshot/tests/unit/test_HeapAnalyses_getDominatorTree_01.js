/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the HeapAnalyses{Client,Worker} "getDominatorTree" request.

function run_test() {
  run_next_test();
}

add_task(function* () {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  yield client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const dominatorTreeId = yield client.computeDominatorTree(snapshotFilePath);
  equal(typeof dominatorTreeId, "number",
        "should get a dominator tree id, and it should be a number");

  const partialTree = yield client.getDominatorTree({ dominatorTreeId });
  ok(partialTree, "Should get a partial tree");
  equal(typeof partialTree, "object",
        "partialTree should be an object");

  function checkTree(node) {
    equal(typeof node.nodeId, "number", "each node should have an id");

    if (node === partialTree) {
      equal(node.parentId, undefined, "the root has no parent");
    } else {
      equal(typeof node.parentId, "number", "each node should have a parent id");
    }

    equal(typeof node.retainedSize, "number",
          "each node should have a retained size");

    ok(node.children === undefined || Array.isArray(node.children),
       "each node either has a list of children, or undefined meaning no children loaded");
    equal(typeof node.moreChildrenAvailable, "boolean",
          "each node should indicate if there are more children available or not");

    if (node.children) {
      node.children.forEach(checkTree);
    }
  }

  checkTree(partialTree);

  client.destroy();
});
