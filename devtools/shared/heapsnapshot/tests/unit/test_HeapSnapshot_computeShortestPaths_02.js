/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test computing shortest paths with invalid arguments.

function run_test() {
  const path = ChromeUtils.saveHeapSnapshot({ runtime: true });
  const snapshot = ChromeUtils.readHeapSnapshot(path);

  const dominatorTree = snapshot.computeDominatorTree();
  const target = dominatorTree.getImmediatelyDominated(dominatorTree.root).pop();
  ok(target);

  let threw = false;
  try {
    snapshot.computeShortestPaths(0, [target], 2);
  } catch (_) {
    threw = true;
  }
  ok(threw, "invalid start node should throw");

  threw = false;
  try {
    snapshot.computeShortestPaths(dominatorTree.root, [0], 2);
  } catch (_) {
    threw = true;
  }
  ok(threw, "invalid target nodes should throw");

  threw = false;
  try {
    snapshot.computeShortestPaths(dominatorTree.root, [], 2);
  } catch (_) {
    threw = true;
  }
  ok(threw, "empty target nodes should throw");

  threw = false;
  try {
    snapshot.computeShortestPaths(dominatorTree.root, [target], 0);
  } catch (_) {
    threw = true;
  }
  ok(threw, "0 max paths should throw");

  do_test_finished();
}
