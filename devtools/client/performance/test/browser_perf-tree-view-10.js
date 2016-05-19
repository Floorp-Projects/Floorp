/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler's tree view, when inverted, displays the self and
 * total costs correctly.
 */

const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
const { CallView } = require("devtools/client/performance/modules/widgets/tree-view");
const RecordingUtils = require("devtools/shared/performance/recording-utils");

add_task(function () {
  let threadNode = new ThreadNode(gProfile.threads[0], { startTime: 0, endTime: 50, invertTree: true });
  let treeRoot = new CallView({ frame: threadNode, inverted: true });
  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  // Add 1 to each index to skip the hidden root node
  let $$nam = i => container.querySelectorAll(".call-tree-cell[type=function] > .call-tree-name")[i + 1];
  let $$per = i => container.querySelectorAll(".call-tree-cell[type=percentage]")[i + 1];
  let $$selfper = i => container.querySelectorAll(".call-tree-cell[type='self-percentage']")[i + 1];

  /**
   * Samples:
   *
   * A->C
   * A->B
   * A->B->C x4
   * A->B->D x4
   *
   * Expected:
   *
   * +--total--+--self--+--tree----+
   * |   50%   |   50%  |  C       |
   * |   40%   |   0    |  -> B    |
   * |   30%   |   0    |     -> A |
   * |   10%   |   0    |  -> A    |
   * |   40%   |   40%  |  D       |
   * |   40%   |   0    |  -> B    |
   * |   40%   |   0    |     -> A |
   * |   10%   |   10%  |  B       |
   * |   10%   |   0    |  -> A    |
   * +---------+--------+----------+
   */

  is(container.childNodes.length, 10,
    "The container node should have all children available.");

  [ // total, self, indent + name
    [ 50, 50, "C"],
    [ 40, 0, "  B"],
    [ 30, 0, "    A"],
    [ 10, 0, "  A"],
    [ 40, 40, "D"],
    [ 40, 0, "  B"],
    [ 40, 0, "    A"],
    [ 10, 10, "B"],
    [ 10, 0, "  A"],
  ].forEach(function (def, i) {
    info(`Checking ${i}th tree item.`);

    let [total, self, name] = def;
    name = name.trim();

    is($$nam(i).textContent.trim(), name, `${name} has correct name.`);
    is($$per(i).textContent.trim(), `${total}%`, `${name} has correct total percent.`);
    is($$selfper(i).textContent.trim(), `${self}%`, `${name} has correct self percent.`);
  });
});

const gProfile = RecordingUtils.deflateProfile({
  meta: { version: 2 },
  threads: [{
    samples: [{
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
    }]
  }]
});
