/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler's tree view implementation works properly and
 * creates the correct DOM nodes in the correct order.
 */

var { CATEGORY_MASK } = require("devtools/client/performance/modules/global");

function test() {
  let { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  let { CallView } = require("devtools/client/performance/modules/widgets/tree-view");

  let threadNode = new ThreadNode(gThread, { startTime: 0, endTime: 20 });
  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;
  let treeRoot = new CallView({ frame: threadNode });

  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  is(treeRoot.target.getAttribute("origin"), "chrome",
    "The root node's 'origin' attribute is correct.");
  is(treeRoot.target.getAttribute("category"), "",
    "The root node's 'category' attribute is correct.");
  is(treeRoot.target.getAttribute("tooltiptext"), "",
    "The root node's 'tooltiptext' attribute is correct.");
  is(treeRoot.target.querySelector(".call-tree-category"), null,
    "The root node's category label cell should be hidden.");

  let A = treeRoot.getChild();
  let B = A.getChild();
  let D = B.getChild();

  is(D.target.getAttribute("origin"), "chrome",
    "The .A.B.D node's 'origin' attribute is correct.");
  is(D.target.getAttribute("category"), "gc",
    "The .A.B.D node's 'category' attribute is correct.");
  is(D.target.getAttribute("tooltiptext"), "D (http://foo/bar/baz:78:1337)",
    "The .A.B.D node's 'tooltiptext' attribute is correct.");
  ok(!A.target.querySelector(".call-tree-category").hidden,
    "The .A.B.D node's category label cell should not be hidden.");

  is(D.target.childNodes.length, 6,
    "The number of columns displayed for tree items is correct.");
  is(D.target.childNodes[0].getAttribute("type"), "duration",
    "The first column displayed for tree items is correct.");
  is(D.target.childNodes[1].getAttribute("type"), "percentage",
    "The third column displayed for tree items is correct.");
  is(D.target.childNodes[2].getAttribute("type"), "self-duration",
    "The second column displayed for tree items is correct.");
  is(D.target.childNodes[3].getAttribute("type"), "self-percentage",
    "The fourth column displayed for tree items is correct.");
  is(D.target.childNodes[4].getAttribute("type"), "samples",
    "The fifth column displayed for tree items is correct.");
  is(D.target.childNodes[5].getAttribute("type"), "function",
    "The sixth column displayed for tree items is correct.");

  let functionCell = D.target.childNodes[5];

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
  is(functionCell.childNodes[4].className, "plain call-tree-column",
    "The fifth node displayed for function cells is correct.");
  is(functionCell.childNodes[5].className, "plain call-tree-host",
    "The sixth node displayed for function cells is correct.");
  is(functionCell.childNodes[6].tagName, "spacer",
    "The seventh node displayed for function cells is correct.");
  is(functionCell.childNodes[7].className, "plain call-tree-category",
    "The eight node displayed for function cells is correct.");

  finish();
}

var gThread = synthesizeProfileForTest([{
  time: 5,
  frames: [
    { category: CATEGORY_MASK('other'),  location: "(root)" },
    { category: CATEGORY_MASK('other'),  location: "A (http://foo/bar/baz:12)" },
    { category: CATEGORY_MASK('css'),    location: "B (http://foo/bar/baz:34)" },
    { category: CATEGORY_MASK('js'),     location: "C (http://foo/bar/baz:56)" }
  ]
}, {
  time: 5 + 1,
  frames: [
    { category: CATEGORY_MASK('other'),  location: "(root)" },
    { category: CATEGORY_MASK('other'),  location: "A (http://foo/bar/baz:12)" },
    { category: CATEGORY_MASK('css'),    location: "B (http://foo/bar/baz:34)" },
    { category: CATEGORY_MASK('gc', 1),  location: "D (http://foo/bar/baz:78:1337)" }
  ]
}, {
  time: 5 + 1 + 2,
  frames: [
    { category: CATEGORY_MASK('other'),  location: "(root)" },
    { category: CATEGORY_MASK('other'),  location: "A (http://foo/bar/baz:12)" },
    { category: CATEGORY_MASK('css'),    location: "B (http://foo/bar/baz:34)" },
    { category: CATEGORY_MASK('gc', 1),  location: "D (http://foo/bar/baz:78:1337)" }
  ]
}, {
  time: 5 + 1 + 2 + 7,
  frames: [
    { category: CATEGORY_MASK('other'),   location: "(root)" },
    { category: CATEGORY_MASK('other'),   location: "A (http://foo/bar/baz:12)" },
    { category: CATEGORY_MASK('gc', 2),   location: "E (http://foo/bar/baz:90)" },
    { category: CATEGORY_MASK('network'), location: "F (http://foo/bar/baz:99)" }
  ]
}]);
