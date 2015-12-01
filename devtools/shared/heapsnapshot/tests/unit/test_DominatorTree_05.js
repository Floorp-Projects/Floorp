/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can get the set of immediately dominated nodes for any given
// node and that this forms a tree.

function run_test() {
  var dominatorTree = saveHeapSnapshotAndComputeDominatorTree();
  equal(typeof dominatorTree.getImmediatelyDominated, "function",
        "getImmediatelyDominated should be a function");

  // Do a traversal of the dominator tree.
  //
  // Note that we don't assert directly, only if we get an unexpected
  // value. There are just way too many nodes in the heap graph to assert for
  // every one. This test would constantly time out and assertion messages would
  // overflow the log size.

  var root = dominatorTree.root;
  equal(dominatorTree.getImmediateDominator(root), null,
        "The root should not have a parent");

  var seen = new Set();
  var stack = [root];
  while (stack.length > 0) {
    var top = stack.pop();

    if (seen.has(top)) {
      ok(false,
         "This is a tree, not a graph: we shouldn't have multiple edges to the same node");
    }
    seen.add(top);
    if (seen.size % 1000 === 0) {
      dumpn("Progress update: seen size = " + seen.size);
    }

    var newNodes = dominatorTree.getImmediatelyDominated(top);
    if (Object.prototype.toString.call(newNodes) !== "[object Array]") {
      ok(false, "getImmediatelyDominated should return an array for known node ids");
    }

    var topSize = dominatorTree.getRetainedSize(top);

    var lastSize = Infinity;
    for (var i = 0; i < newNodes.length; i++) {
      if (typeof newNodes[i] !== "number") {
        ok(false, "Every dominated id should be a number");
      }

      if (dominatorTree.getImmediateDominator(newNodes[i]) !== top) {
        ok(false, "child's parent should be the expected parent");
      }

      var thisSize = dominatorTree.getRetainedSize(newNodes[i]);

      if (thisSize >= topSize) {
        ok(false, "the size of children in the dominator tree should always be less than that of their parent");
      }

      if (thisSize > lastSize) {
        ok(false,
           "children should be sorted by greatest to least retained size, "
           + "lastSize = " + lastSize + ", thisSize = " + thisSize);
      }

      lastSize = thisSize;
      stack.push(newNodes[i]);
    }
  }

  ok(true, "Successfully walked the tree");
  dumpn("Walked " + seen.size + " nodes");

  do_test_finished();
}
