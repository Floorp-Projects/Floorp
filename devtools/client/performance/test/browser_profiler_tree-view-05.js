/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler's tree view implementation works properly and
 * can toggle categories hidden or visible.
 */

function test() {
  let { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  let { CallView } = require("devtools/client/performance/modules/widgets/tree-view");

  let threadNode = new ThreadNode(gThread, { startTime: 0, endTime: 20 });
  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;
  let treeRoot = new CallView({ frame: threadNode });

  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  let categories = container.querySelectorAll(".call-tree-category");
  is(categories.length, 5,
    "The call tree displays a correct number of categories.");
  ok(!container.hasAttribute("categories-hidden"),
    "All categories should be visible in the tree.");

  treeRoot.toggleCategories(false);
  is(categories.length, 5,
    "The call tree displays the same number of categories.");
  ok(container.hasAttribute("categories-hidden"),
    "All categories should now be hidden in the tree.");

  finish();
}

var gThread = synthesizeProfileForTest([{
  time: 5,
  frames: [
    { category: 8,  location: "(root)" },
    { category: 8,  location: "A (http://foo/bar/baz:12)" },
    { category: 16, location: "B (http://foo/bar/baz:34)" },
    { category: 32, location: "C (http://foo/bar/baz:56)" }
  ]
}, {
  time: 5 + 1,
  frames: [
    { category: 8,  location: "(root)" },
    { category: 8,  location: "A (http://foo/bar/baz:12)" },
    { category: 16, location: "B (http://foo/bar/baz:34)" },
    { category: 64, location: "D (http://foo/bar/baz:78)" }
  ]
}, {
  time: 5 + 1 + 2,
  frames: [
    { category: 8,  location: "(root)" },
    { category: 8,  location: "A (http://foo/bar/baz:12)" },
    { category: 16, location: "B (http://foo/bar/baz:34)" },
    { category: 64, location: "D (http://foo/bar/baz:78)" }
  ]
}, {
  time: 5 + 1 + 2 + 7,
  frames: [
    { category: 8,   location: "(root)" },
    { category: 8,   location: "A (http://foo/bar/baz:12)" },
    { category: 128, location: "E (http://foo/bar/baz:90)" },
    { category: 256, location: "F (http://foo/bar/baz:99)" }
  ]
}]);
