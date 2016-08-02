/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* exported Cc, Ci, Cu, Cr, Services, console, PLATFORM_DATA_PREF, getFrameNodePath,
   synthesizeProfileForTest */
var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
var Services = require("Services");
var { console } = require("resource://gre/modules/Console.jsm");
const RecordingUtils = require("devtools/shared/performance/recording-utils");
const PLATFORM_DATA_PREF = "devtools.performance.ui.show-platform-data";

/**
 * Get a path in a FrameNode call tree.
 */
function getFrameNodePath(root, path) {
  let calls = root.calls;
  let foundNode;
  for (let key of path.split(" > ")) {
    foundNode = calls.find((node) => node.key == key);
    if (!foundNode) {
      break;
    }
    calls = foundNode.calls;
  }
  return foundNode;
}

/**
 * Synthesize a profile for testing.
 */
function synthesizeProfileForTest(samples) {
  samples.unshift({
    time: 0,
    frames: [
      { location: "(root)" }
    ]
  });

  let uniqueStacks = new RecordingUtils.UniqueStacks();
  return RecordingUtils.deflateThread({
    samples: samples,
    markers: []
  }, uniqueStacks);
}
