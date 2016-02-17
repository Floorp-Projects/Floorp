/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sanity test that we can compute shortest paths.
//
// Because the actual heap graph is too unpredictable and likely to drastically
// change as various implementation bits change, we don't test exact paths
// here. See js/src/jsapi-tests/testUbiNode.cpp for such tests, where we can
// control the specific graph shape and structure and so testing exact paths is
// reliable.

function run_test() {
  const path = ChromeUtils.saveHeapSnapshot({ runtime: true });
  const snapshot = ChromeUtils.readHeapSnapshot(path);

  const dominatorTree = snapshot.computeDominatorTree();
  const dominatedByRoot = dominatorTree.getImmediatelyDominated(dominatorTree.root)
                                       .slice(0, 10);
  ok(dominatedByRoot);
  ok(dominatedByRoot.length);

  const targetSet = new Set(dominatedByRoot);

  const shortestPaths = snapshot.computeShortestPaths(dominatorTree.root, dominatedByRoot, 2);
  ok(shortestPaths);
  ok(shortestPaths instanceof Map);
  ok(shortestPaths.size === targetSet.size);

  for (let [target, paths] of shortestPaths) {
    ok(targetSet.has(target),
       "We should only get paths for our targets");
    targetSet.delete(target);

    ok(paths.length > 0,
       "We must have at least one path, since the target is dominated by the root");
    ok(paths.length <= 2,
       "Should not have recorded more paths than the max requested");

    dumpn("---------------------");
    dumpn("Shortest paths for 0x" + target.toString(16) + ":");
    for (let path of paths) {
      dumpn("    path =");
      for (let part of path) {
        dumpn("        predecessor: 0x" + part.predecessor.toString(16) +
              "; edge: " + part.edge);
      }
    }
    dumpn("---------------------");

    for (let path of paths) {
      ok(path.length > 0, "Cannot have zero length paths");
      ok(path[0].predecessor === dominatorTree.root,
         "The first predecessor is always our start node");

      for (let part of path) {
        ok(part.predecessor, "Each part of a path has a predecessor");
        ok(!!snapshot.describeNode({ by: "count", count: true, bytes: true},
                                   part.predecessor),
           "The predecessor is in the heap snapshot");
        ok("edge" in part, "Each part has an (potentially null) edge property");
      }
    }
  }

  ok(targetSet.size === 0,
     "We found paths for all of our targets");

  do_test_finished();
}
