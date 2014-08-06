/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler's tree view implementation works properly and
 * has the correct 'root', 'parent', 'level' etc. accessors on child nodes.
 */

function test() {
  let { ThreadNode } = devtools.require("devtools/profiler/tree-model");
  let { CallView } = devtools.require("devtools/profiler/tree-view");

  let threadNode = new ThreadNode(gSamples);
  let treeRoot = new CallView({ frame: threadNode });

  let container = document.createElement("vbox");
  container.id = "call-tree-container";
  treeRoot.attachTo(container);

  let A = treeRoot.getChild();
  let B = A.getChild();
  let C = B.getChild();

  is(C.root, treeRoot,
    "The .A.B.C node has the correct root.");
  is(C.parent, B,
    "The .A.B.C node has the correct parent.");
  is(C.level, 3,
    "The .A.B.C node has the correct level.");
  is(C.target.className, "call-tree-item",
    "The .A.B.C node has the correct target node.");
  is(C.container.id, "call-tree-container",
    "The .A.B.C node has the correct container node.");

  finish();
}

let gSamples = [{
  time: 5,
  frames: [
    { category: 8,  location: "(root)" },
    { category: 8,  location: "A (http://foo/bar/baz:12)" },
    { category: 16, location: "B (http://foo/bar/baz:34)" },
    { category: 32, location: "C (http://foo/bar/baz:56)" }
  ]
}, {
  time: 5 + 6,
  frames: [
    { category: 8,  location: "(root)" },
    { category: 8,  location: "A (http://foo/bar/baz:12)" },
    { category: 16, location: "B (http://foo/bar/baz:34)" },
    { category: 64, location: "D (http://foo/bar/baz:78)" }
  ]
}, {
  time: 5 + 6 + 7,
  frames: [
    { category: 8,   location: "(root)" },
    { category: 8,   location: "A (http://foo/bar/baz:12)" },
    { category: 128, location: "E (http://foo/bar/baz:90)" },
    { category: 256, location: "F (http://foo/bar/baz:99)" }
  ]
}];
