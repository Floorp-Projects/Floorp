/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if allocations data received from the performance actor is properly
 * converted to something that follows the same structure as the samples data
 * received from the profiler.
 */

function run_test() {
  run_next_test();
}

add_task(function () {
  const { getProfileThreadFromAllocations } = require("devtools/shared/performance/recording-utils");
  let output = getProfileThreadFromAllocations(TEST_DATA);
  equal(output.toSource(), EXPECTED_OUTPUT.toSource(), "The output is correct.");
});

var TEST_DATA = {
  sites: [0, 0, 1, 2, 3],
  timestamps: [50, 100, 150, 200, 250],
  sizes: [0, 0, 100, 200, 300],
  frames: [
    null, {
      source: "A",
      line: 1,
      column: 2,
      functionDisplayName: "x",
      parent: 0
    }, {
      source: "B",
      line: 3,
      column: 4,
      functionDisplayName: "y",
      parent: 1
    }, {
      source: "C",
      line: 5,
      column: 6,
      functionDisplayName: null,
      parent: 2
    }
  ]
};

/* eslint-disable no-inline-comments */
var EXPECTED_OUTPUT = {
  name: "allocations",
  samples: {
    "schema": {
      "stack": 0,
      "time": 1,
      "size": 2,
    },
    data: [
      [ 1, 150, 100 ],
      [ 2, 200, 200 ],
      [ 3, 250, 300 ]
    ]
  },
  stackTable: {
    "schema": {
      "prefix": 0,
      "frame": 1
    },
    "data": [
      null,
      [ null, 1 ], // x (A:1:2)
      [ 1, 2 ],    // x (A:1:2) > y (B:3:4)
      [ 2, 3 ]     // x (A:1:2) > y (B:3:4) > C:5:6
    ]
  },
  frameTable: {
    "schema": {
      "location": 0,
      "implementation": 1,
      "optimizations": 2,
      "line": 3,
      "category": 4
    },
    data: [
      null,
      [ 0 ],
      [ 1 ],
      [ 2 ]
    ]
  },
  "stringTable": [
    "x (A:1:2)",
    "y (B:3:4)",
    "C:5:6"
  ],
};
/* eslint-enable no-inline-comments */
