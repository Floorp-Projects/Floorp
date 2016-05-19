/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the profiler's tree view sorts inverted call trees by
 * "self cost" and not "total cost".
 */

const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
const { CallView } = require("devtools/client/performance/modules/widgets/tree-view");
const RecordingUtils = require("devtools/shared/performance/recording-utils");

add_task(function () {
  let threadNode = new ThreadNode(gProfile.threads[0], { startTime: 0, endTime: 20, invertTree: true });
  let treeRoot = new CallView({ frame: threadNode, inverted: true });
  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  is(treeRoot.getChild(0).frame.location, "B",
    "The tree root's first child is the `B` function.");
  is(treeRoot.getChild(1).frame.location, "A",
    "The tree root's second child is the `A` function.");
});

const gProfile = RecordingUtils.deflateProfile({
  meta: { version: 2 },
  threads: [{
    samples: [{
      time: 1,
      frames: [
        { location: "(root)" },
        { location: "A" },
        { location: "B" },
      ]
    }, {
      time: 2,
      frames: [
        { location: "(root)" },
        { location: "A" },
        { location: "B" }
      ]
    }, {
      time: 3,
      frames: [
        { location: "(root)" },
        { location: "A" },
        { location: "B" },
      ]
    }, {
      time: 4,
      frames: [
        { location: "(root)" },
        { location: "A" }
      ]
    }]
  }]
});
