/* Any copyright is dedicated to the Public Domain.
   http://foo/bar/creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler's tree view implementation works properly and
 * creates the correct column structure.
 */

function test() {
  let { ThreadNode } = devtools.require("devtools/shared/profiler/tree-model");
  let { CallView } = devtools.require("devtools/shared/profiler/tree-view");

  let threadNode = new ThreadNode(gSamples);
  let treeRoot = new CallView({ frame: threadNode });

  let container = document.createElement("vbox");
  treeRoot.autoExpandDepth = 0;
  treeRoot.attachTo(container);

  is(container.childNodes.length, 1,
    "The container node should have one child available.");
  is(container.childNodes[0].className, "call-tree-item",
    "The root node in the tree has the correct class name.");

  is(container.childNodes[0].childNodes.length, 8,
    "The root node in the tree has the correct number of children.");
  is(container.childNodes[0].querySelectorAll(".call-tree-cell").length, 8,
    "The root node in the tree has only 'call-tree-cell' children.");

  is(container.childNodes[0].childNodes[0].getAttribute("type"), "duration",
    "The root node in the tree has a duration cell.");
  is(container.childNodes[0].childNodes[0].getAttribute("value"), "15 ms",
    "The root node in the tree has the correct duration cell value.");

  is(container.childNodes[0].childNodes[1].getAttribute("type"), "percentage",
    "The root node in the tree has a percentage cell.");
  is(container.childNodes[0].childNodes[1].getAttribute("value"), "100%",
    "The root node in the tree has the correct percentage cell value.");

  is(container.childNodes[0].childNodes[2].getAttribute("type"), "allocations",
    "The root node in the tree has a self-duration cell.");
  is(container.childNodes[0].childNodes[2].getAttribute("value"), "0",
    "The root node in the tree has the correct self-duration cell value.");

  is(container.childNodes[0].childNodes[3].getAttribute("type"), "self-duration",
    "The root node in the tree has a self-duration cell.");
  is(container.childNodes[0].childNodes[3].getAttribute("value"), "0 ms",
    "The root node in the tree has the correct self-duration cell value.");

  is(container.childNodes[0].childNodes[4].getAttribute("type"), "self-percentage",
    "The root node in the tree has a self-percentage cell.");
  is(container.childNodes[0].childNodes[4].getAttribute("value"), "0%",
    "The root node in the tree has the correct self-percentage cell value.");

  is(container.childNodes[0].childNodes[5].getAttribute("type"), "self-allocations",
    "The root node in the tree has a self-percentage cell.");
  is(container.childNodes[0].childNodes[5].getAttribute("value"), "0",
    "The root node in the tree has the correct self-percentage cell value.");

  is(container.childNodes[0].childNodes[6].getAttribute("type"), "samples",
    "The root node in the tree has an samples cell.");
  is(container.childNodes[0].childNodes[6].getAttribute("value"), "4",
    "The root node in the tree has the correct samples cell value.");

  is(container.childNodes[0].childNodes[7].getAttribute("type"), "function",
    "The root node in the tree has a function cell.");
  is(container.childNodes[0].childNodes[7].style.MozMarginStart, "0px",
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
}];
