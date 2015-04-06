/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when displaying only content nodes, platform nodes are generalized.
 */

let { CATEGORY_MASK } = devtools.require("devtools/shared/profiler/global");

function test() {
  let { ThreadNode } = devtools.require("devtools/shared/profiler/tree-model");
  let url = (n) => `http://content/${n}`;

  // Create a root node from a given samples array.

  let root = new ThreadNode(gSamples, { contentOnly: true });

  /*
   * should have a tree like:
   * root
   *   - (JS)
   *   - A
   *     - (GC)
   *     - B
   *       - C
   *       - D
   *     - E
   *       - F
   *       - (JS)
   */

  // Test the root node.

  is(Object.keys(root.calls).length, 2, "root has 2 children");
  ok(root.calls[url("A")], "root has content child");
  ok(root.calls["64"], "root has platform generalized child");
  is(Object.keys(root.calls["64"].calls).length, 0, "platform generalized child is a leaf.");

  ok(root.calls[url("A")].calls["128"], "A has platform generalized child of another type");
  is(Object.keys(root.calls[url("A")].calls["128"].calls).length, 0, "second generalized type is a leaf.");

  ok(root.calls[url("A")].calls[url("E")].calls[url("F")].calls["64"],
    "a second leaf of the first generalized type exists deep in the tree.");
  ok(root.calls[url("A")].calls["128"], "A has platform generalized child of another type");

  is(root.calls["64"].category, root.calls[url("A")].calls[url("E")].calls[url("F")].calls["64"].category,
    "generalized frames of same type are duplicated in top-down view");
  finish();
}

let gSamples = [{
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
}];
