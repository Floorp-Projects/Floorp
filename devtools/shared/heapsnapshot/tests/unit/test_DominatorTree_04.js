/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can get the retained sizes of dominator trees.

function run_test() {
  const dominatorTree = saveHeapSnapshotAndComputeDominatorTree();
  equal(typeof dominatorTree.getRetainedSize, "function",
        "getRetainedSize should be a function");

  const size = dominatorTree.getRetainedSize(dominatorTree.root);
  ok(size, "should get a size for the root");
  equal(typeof size, "number", "retained sizes should be a number");
  equal(Math.floor(size), size, "size should be an integer");
  ok(size > 0, "size should be positive");
  ok(size <= Math.pow(2, 64), "size should be less than or equal to 2^64");

  const bad = dominatorTree.getRetainedSize(1);
  equal(bad, null, "null is returned for unknown node ids");

  do_test_finished();
}
