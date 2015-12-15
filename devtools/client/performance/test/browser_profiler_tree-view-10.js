/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler's tree view, when inverted, displays the self and
 * total costs correctly.
 */

function test() {
  let { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  let { CallView } = require("devtools/client/performance/modules/widgets/tree-view");

  let threadNode = new ThreadNode(gThread, { invertTree: true, startTime: 0, endTime: 50 });
  let treeRoot = new CallView({ frame: threadNode, inverted: true, hidden: true });

  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  // Add 1 to each index to skip the hidden root node
  let $$fun = i => container.querySelectorAll(".call-tree-cell[type=function]")[i+1];
  let $$name = i => container.querySelectorAll(".call-tree-cell[type=function] > .call-tree-name")[i+1];
  let $$percentage = i => container.querySelectorAll(".call-tree-cell[type=percentage]")[i+1];
  let $$selfpercentage = i => container.querySelectorAll(".call-tree-cell[type='self-percentage']")[i+1];

  /**
   * Samples
   *
   * A->C
   * A->B
   * A->B->C x4
   * A->B->D x4
   *
   * Expected Tree
   * +--total--+--self--+--tree-------------+
   * |   50%   |   50%  |  C
   * |   40%   |   0    |  -> B
   * |   30%   |   0    |     -> A
   * |   10%   |   0    |  -> A
   *
   * |   40%   |   40%  |  D
   * |   40%   |   0    |  -> B
   * |   40%   |   0    |     -> A
   *
   * |   10%   |   10%  |  B
   * |   10%   |   0    |  -> A
   */

  is(container.childNodes.length, 10,
    "The container node should have all children available.");

  [ // total, self, indent + name
    [ 50, 50, "C"],
    [ 40,  0, "  B"],
    [ 30,  0, "    A"],
    [ 10,  0, "  A"],
    [ 40, 40, "D"],
    [ 40,  0, "  B"],
    [ 40,  0, "    A"],
    [ 10, 10, "B"],
    [ 10,  0, "  A"],
  ].forEach(function (def, i) {
    info(`Checking ${i}th tree item`);
    let [total, self, name] = def;
    name = name.trim();

    is($$name(i).textContent.trim(), name, `${name} has correct name.`);
    is($$percentage(i).textContent.trim(), `${total}%`, `${name} has correct total percent.`);
    is($$selfpercentage(i).textContent.trim(), `${self}%`, `${name} has correct self percent.`);
  });

  finish();
}

var gThread = synthesizeProfileForTest([{
  time: 5,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "C" }
  ]
}, {
  time: 10,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "D" }
  ]
}, {
  time: 15,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "C" },
  ]
}, {
  time: 20,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
  ]
}, {
  time: 25,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "C" }
  ]
}, {
  time: 30,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "C" }
  ]
}, {
  time: 35,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "D" }
  ]
}, {
  time: 40,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "D" }
  ]
}, {
  time: 45,
  frames: [
    { location: "(root)" },
    { location: "B" },
    { location: "C" }
  ]
}, {
  time: 50,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "D" }
  ]
}]);
