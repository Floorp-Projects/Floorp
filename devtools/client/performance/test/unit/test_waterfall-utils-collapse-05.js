/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the waterfall collapsing logic works properly
 * when dealing with OTMT markers.
 */

function run_test() {
  run_next_test();
}

add_task(function test() {
  const WaterfallUtils = require("devtools/client/performance/modules/logic/waterfall-utils");

  let rootMarkerNode = WaterfallUtils.createParentNode({ name: "(root)" });

  WaterfallUtils.collapseMarkersIntoNode({
    rootNode: rootMarkerNode,
    markersList: gTestMarkers
  });

  compare(rootMarkerNode, gExpectedOutput);

  function compare(marker, expected) {
    for (let prop in expected) {
      if (prop === "submarkers") {
        for (let i = 0; i < expected.submarkers.length; i++) {
          compare(marker.submarkers[i], expected.submarkers[i]);
        }
      } else if (prop !== "uid") {
        equal(marker[prop], expected[prop], `${expected.name} matches ${prop}`);
      }
    }
  }
});

const gTestMarkers = [
  { start: 1, end: 4, name: "A1-mt", processType: 1, isOffMainThread: false },
  // This should collapse only under A1-mt
  { start: 2, end: 3, name: "B1", processType: 1, isOffMainThread: false },
  // This should never collapse.
  { start: 2, end: 3, name: "C1", processType: 1, isOffMainThread: true },

  { start: 5, end: 8, name: "A1-otmt", processType: 1, isOffMainThread: true },
  // This should collapse only under A1-mt
  { start: 6, end: 7, name: "B2", processType: 1, isOffMainThread: false },
  // This should never collapse.
  { start: 6, end: 7, name: "C2", processType: 1, isOffMainThread: true },

  { start: 9, end: 12, name: "A2-mt", processType: 2, isOffMainThread: false },
  // This should collapse only under A2-mt
  { start: 10, end: 11, name: "D1", processType: 2, isOffMainThread: false },
  // This should never collapse.
  { start: 10, end: 11, name: "E1", processType: 2, isOffMainThread: true },

  { start: 13, end: 16, name: "A2-otmt", processType: 2, isOffMainThread: true },
  // This should collapse only under A2-mt
  { start: 14, end: 15, name: "D2", processType: 2, isOffMainThread: false },
  // This should never collapse.
  { start: 14, end: 15, name: "E2", processType: 2, isOffMainThread: true },

  // This should not collapse, because there's no parent in this process.
  { start: 14, end: 15, name: "F", processType: 3, isOffMainThread: false },

  // This should never collapse.
  { start: 14, end: 15, name: "G", processType: 3, isOffMainThread: true },
];

const gExpectedOutput = {
  name: "(root)",
  submarkers: [{
    start: 1,
    end: 4,
    name: "A1-mt",
    processType: 1,
    isOffMainThread: false,
    submarkers: [{
      start: 2,
      end: 3,
      name: "B1",
      processType: 1,
      isOffMainThread: false
    }]
  }, {
    start: 2,
    end: 3,
    name: "C1",
    processType: 1,
    isOffMainThread: true
  }, {
    start: 5,
    end: 8,
    name: "A1-otmt",
    processType: 1,
    isOffMainThread: true,
    submarkers: [{
      start: 6,
      end: 7,
      name: "B2",
      processType: 1,
      isOffMainThread: false
    }]
  }, {
    start: 6,
    end: 7,
    name: "C2",
    processType: 1,
    isOffMainThread: true
  }, {
    start: 9,
    end: 12,
    name: "A2-mt",
    processType: 2,
    isOffMainThread: false,
    submarkers: [{
      start: 10,
      end: 11,
      name: "D1",
      processType: 2,
      isOffMainThread: false
    }]
  }, {
    start: 10,
    end: 11,
    name: "E1",
    processType: 2,
    isOffMainThread: true
  }, {
    start: 13,
    end: 16,
    name: "A2-otmt",
    processType: 2,
    isOffMainThread: true,
    submarkers: [{
      start: 14,
      end: 15,
      name: "D2",
      processType: 2,
      isOffMainThread: false
    }]
  }, {
    start: 14,
    end: 15,
    name: "E2",
    processType: 2,
    isOffMainThread: true
  }, {
    start: 14,
    end: 15,
    name: "F",
    processType: 3,
    isOffMainThread: false,
    submarkers: []
  }, {
    start: 14,
    end: 15,
    name: "G",
    processType: 3,
    isOffMainThread: true,
    submarkers: []
  }]
};
