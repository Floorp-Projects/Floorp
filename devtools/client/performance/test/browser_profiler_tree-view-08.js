/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the profiler's tree view renders generalized platform data
 * when `contentOnly` is on correctly.
 */

var { CATEGORY_MASK } = require("devtools/client/performance/modules/global");

function test() {
  let { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  let { CallView } = require("devtools/client/performance/modules/widgets/tree-view");

  /*
   * should have a tree like:
   * root
   *   - A
   *     - B
   *       - C
   *       - D
   *     - E
   *       - F
   *         - (JS)
   *     - (GC)
   *   - (JS)
   */

  let threadNode = new ThreadNode(gThread, { startTime: 0, endTime: 30, contentOnly: true });
  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;
  let treeRoot = new CallView({ frame: threadNode, autoExpandDepth: 10 });

  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  let A = treeRoot.getChild(0);
  let JS = treeRoot.getChild(1);
  let GC = A.getChild(1);
  let JS2 = A.getChild(2).getChild().getChild();

  is(JS.target.getAttribute("category"), "js",
    "Generalized JS node has correct category");
  is(JS.target.getAttribute("tooltiptext"), "JIT",
    "Generalized JS node has correct category");
  is(JS.target.querySelector(".call-tree-name").textContent.trim(), "JIT",
    "Generalized JS node has correct display value as just the category name.");

  is(JS2.target.getAttribute("category"), "js",
    "Generalized second JS node has correct category");
  is(JS2.target.getAttribute("tooltiptext"), "JIT",
    "Generalized second JS node has correct category");
  is(JS2.target.querySelector(".call-tree-name").textContent.trim(), "JIT",
    "Generalized second JS node has correct display value as just the category name.");

  is(GC.target.getAttribute("category"), "gc",
    "Generalized GC node has correct category");
  is(GC.target.getAttribute("tooltiptext"), "GC",
    "Generalized GC node has correct category");
  is(GC.target.querySelector(".call-tree-name").textContent.trim(), "GC",
    "Generalized GC node has correct display value as just the category name.");

  finish();
}

var gThread = synthesizeProfileForTest([{
  time: 5,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "http://content/B" },
    { location: "http://content/C" }
  ]
}, {
  time: 5 + 6,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "http://content/B" },
    { location: "contentY", category: CATEGORY_MASK("css") },
    { location: "http://content/D" }
  ]
}, {
  time: 5 + 6 + 7,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "contentY", category: CATEGORY_MASK("css") },
    { location: "http://content/E" },
    { location: "http://content/F" },
    { location: "contentY", category: CATEGORY_MASK("js") },
  ]
}, {
  time: 5 + 20,
  frames: [
    { location: "(root)" },
    { location: "contentX", category: CATEGORY_MASK("js") },
  ]
}, {
  time: 5 + 25,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "contentZ", category: CATEGORY_MASK("gc", 1) },
  ]
}]);
