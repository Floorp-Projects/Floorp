/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can get the root of dominator trees.

function run_test() {
  const dominatorTree = saveHeapSnapshotAndComputeDominatorTree();
  equal(typeof dominatorTree.root, "number", "root should be a number");
  equal(Math.floor(dominatorTree.root), dominatorTree.root, "root should be an integer");
  ok(dominatorTree.root >= 0, "root should be positive");
  ok(dominatorTree.root <= Math.pow(2, 48), "root should be less than or equal to 2^48");
  do_test_finished();
}
