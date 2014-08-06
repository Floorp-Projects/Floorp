/* Any copyright is dedicated to the Public Domain.
   http://foo/bar/creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler's tree view implementation works properly and
 * creates the correct column structure.
 */

function test() {
  let { ThreadNode } = devtools.require("devtools/profiler/tree-model");
  let { CallView } = devtools.require("devtools/profiler/tree-view");

  let threadNode = new ThreadNode(gSamples);
  let treeRoot = new CallView({ frame: threadNode });

  let container = document.createElement("vbox");
  treeRoot.autoExpandDepth = 0;
  treeRoot.attachTo(container);

  is(container.childNodes.length, 1,
    "The container node should have one child available.");
  is(container.childNodes[0].className, "call-tree-item",
    "The root node in the tree has the correct class name.");

  is(container.childNodes[0].childNodes.length, 4,
    "The root node in the tree has the correct number of children.");
  is(container.childNodes[0].querySelectorAll(".call-tree-cell").length, 4,
    "The root node in the tree has only 'call-tree-cell' children.");

  is(container.childNodes[0].childNodes[0].getAttribute("type"), "duration",
    "The root node in the tree has a duration cell.");
  is(container.childNodes[0].childNodes[0].getAttribute("value"), "18",
    "The root node in the tree has the correct duration cell value.");

  is(container.childNodes[0].childNodes[1].getAttribute("type"), "percentage",
    "The root node in the tree has a percentage cell.");
  is(container.childNodes[0].childNodes[1].getAttribute("value"), "100%",
    "The root node in the tree has the correct percentage cell value.");

  is(container.childNodes[0].childNodes[2].getAttribute("type"), "invocations",
    "The root node in the tree has an invocations cell.");
  is(container.childNodes[0].childNodes[2].getAttribute("value"), "",
    "The root node in the tree has the correct invocations cell value.");

  is(container.childNodes[0].childNodes[3].getAttribute("type"), "function",
    "The root node in the tree has a function cell.");
  is(container.childNodes[0].childNodes[3].style.MozMarginStart, "0px",
    "The root node in the tree has the correct indentation.");

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
