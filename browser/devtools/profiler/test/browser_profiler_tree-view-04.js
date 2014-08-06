/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler's tree view implementation works properly and
 * creates the correct DOM nodes in the correct order.
 */

function test() {
  let { ThreadNode } = devtools.require("devtools/profiler/tree-model");
  let { CallView } = devtools.require("devtools/profiler/tree-view");

  let threadNode = new ThreadNode(gSamples);
  let treeRoot = new CallView({ frame: threadNode });

  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  is(treeRoot.target.getAttribute("origin"), "chrome",
    "The root node's 'origin' attribute is correct.");
  is(treeRoot.target.getAttribute("category"), "",
    "The root node's 'category' attribute is correct.");
  is(treeRoot.target.getAttribute("tooltiptext"), "",
    "The root node's 'tooltiptext' attribute is correct.");
  ok(treeRoot.target.querySelector(".call-tree-zoom").hidden,
    "The root node's zoom button cell should be hidden.");
  ok(treeRoot.target.querySelector(".call-tree-category").hidden,
    "The root node's category label cell should be hidden.");

  let A = treeRoot.getChild();
  let B = A.getChild();
  let C = B.getChild();

  is(C.target.getAttribute("origin"), "chrome",
    "The .A.B.C node's 'origin' attribute is correct.");
  is(C.target.getAttribute("category"), "gc",
    "The .A.B.C node's 'category' attribute is correct.");
  is(C.target.getAttribute("tooltiptext"), "D (http://foo/bar/baz:78)",
    "The .A.B.C node's 'tooltiptext' attribute is correct.");
  ok(!A.target.querySelector(".call-tree-zoom").hidden,
    "The .A.B.C node's zoom button cell should not be hidden.");
  ok(!A.target.querySelector(".call-tree-category").hidden,
    "The .A.B.C node's category label cell should not be hidden.");

  is(C.target.childNodes.length, 4,
    "The number of columns displayed for tree items is correct.");
  is(C.target.childNodes[0].getAttribute("type"), "duration",
    "The first column displayed for tree items is correct.");
  is(C.target.childNodes[1].getAttribute("type"), "percentage",
    "The second column displayed for tree items is correct.");
  is(C.target.childNodes[2].getAttribute("type"), "invocations",
    "The third column displayed for tree items is correct.");
  is(C.target.childNodes[3].getAttribute("type"), "function",
    "The fourth column displayed for tree items is correct.");

  let functionCell = C.target.childNodes[3];

  is(functionCell.childNodes.length, 8,
    "The number of columns displayed for function cells is correct.");
  is(functionCell.childNodes[0].className, "arrow theme-twisty",
    "The first node displayed for function cells is correct.");
  is(functionCell.childNodes[1].className, "plain call-tree-name",
    "The second node displayed for function cells is correct.");
  is(functionCell.childNodes[2].className, "plain call-tree-url",
    "The third node displayed for function cells is correct.");
  is(functionCell.childNodes[3].className, "plain call-tree-line",
    "The fourth node displayed for function cells is correct.");
  is(functionCell.childNodes[4].className, "plain call-tree-host",
    "The fifth node displayed for function cells is correct.");
  is(functionCell.childNodes[5].className, "plain call-tree-zoom",
    "The sixth node displayed for function cells is correct.");
  is(functionCell.childNodes[6].tagName, "spacer",
    "The seventh node displayed for function cells is correct.");
  is(functionCell.childNodes[7].className, "plain call-tree-category",
    "The eight node displayed for function cells is correct.");

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

