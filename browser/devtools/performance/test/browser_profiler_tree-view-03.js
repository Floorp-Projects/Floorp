/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler's tree view implementation works properly and
 * creates the correct column structure and can auto-expand all nodes.
 */

function test() {
  let { ThreadNode } = devtools.require("devtools/shared/profiler/tree-model");
  let { CallView } = devtools.require("devtools/shared/profiler/tree-view");

  let threadNode = new ThreadNode(gThread);
  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;
  let treeRoot = new CallView({ frame: threadNode });

  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  let $$fun = i => container.querySelectorAll(".call-tree-cell[type=function]")[i];
  let $$name = i => container.querySelectorAll(".call-tree-cell[type=function] > .call-tree-name")[i];
  let $$duration = i => container.querySelectorAll(".call-tree-cell[type=duration]")[i];

  is(container.childNodes.length, 7,
    "The container node should have all children available.");
  is(Array.filter(container.childNodes, e => e.className != "call-tree-item").length, 0,
    "All item nodes in the tree have the correct class name.");

  is($$fun(0).style.MozMarginStart, "0px",
    "The root node's function cell has the correct indentation.");
  is($$fun(1).style.MozMarginStart, "16px",
    "The .A node's function cell has the correct indentation.");
  is($$fun(2).style.MozMarginStart, "32px",
    "The .A.B node's function cell has the correct indentation.");
  is($$fun(3).style.MozMarginStart, "48px",
    "The .A.B.D node's function cell has the correct indentation.");
  is($$fun(4).style.MozMarginStart, "48px",
    "The .A.B.C node's function cell has the correct indentation.");
  is($$fun(5).style.MozMarginStart, "32px",
    "The .A.E node's function cell has the correct indentation.");
  is($$fun(6).style.MozMarginStart, "48px",
    "The .A.E.F node's function cell has the correct indentation.");

  is($$name(0).getAttribute("value"), "(root)",
    "The root node's function cell displays the correct name.");
  is($$name(1).getAttribute("value"), "A",
    "The .A node's function cell displays the correct name.");
  is($$name(2).getAttribute("value"), "B",
    "The .A.B node's function cell displays the correct name.");
  is($$name(3).getAttribute("value"), "D",
    "The .A.B.D node's function cell displays the correct name.");
  is($$name(4).getAttribute("value"), "C",
    "The .A.B.C node's function cell displays the correct name.");
  is($$name(5).getAttribute("value"), "E",
    "The .A.E node's function cell displays the correct name.");
  is($$name(6).getAttribute("value"), "F",
    "The .A.E.F node's function cell displays the correct name.");

  is($$duration(0).getAttribute("value"), "15 ms",
    "The root node's function cell displays the correct duration.");
  is($$duration(1).getAttribute("value"), "15 ms",
    "The .A node's function cell displays the correct duration.");
  is($$duration(2).getAttribute("value"), "8 ms",
    "The .A.B node's function cell displays the correct duration.");
  is($$duration(3).getAttribute("value"), "3 ms",
    "The .A.B.D node's function cell displays the correct duration.");
  is($$duration(4).getAttribute("value"), "5 ms",
    "The .A.B.C node's function cell displays the correct duration.");
  is($$duration(5).getAttribute("value"), "7 ms",
    "The .A.E node's function cell displays the correct duration.");
  is($$duration(6).getAttribute("value"), "7 ms",
    "The .A.E.F node's function cell displays the correct duration.");

  finish();
}

let gThread = synthesizeProfileForTest([{
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

