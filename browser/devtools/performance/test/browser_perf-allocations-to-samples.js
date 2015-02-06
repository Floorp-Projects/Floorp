/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if allocations data received from the memory actor is properly
 * converted to something that follows the same structure as the samples data
 * received from the profiler.
 */

function test() {
  let { RecordingUtils } = devtools.require("devtools/performance/recording-utils");

  let output = RecordingUtils.getSamplesFromAllocations(TEST_DATA);
  is(output.toSource(), EXPECTED_OUTPUT.toSource(), "The output is correct.");

  finish();
}

let TEST_DATA = {
  sites: [0, 0, 1, 2, 3],
  timestamps: [50, 100, 150, 200, 250],
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
  ],
  counts: [11, 22, 33, 44]
};

let EXPECTED_OUTPUT = [{
  time: 50,
  frames: []
}, {
  time: 100,
  frames: []
}, {
  time: 150,
  frames: [{
    location: "x (A:1:2)",
    allocations: 22
  }]
}, {
  time: 200,
  frames: [{
    location: "x (A:1:2)",
    allocations: 22
  }, {
    location: "y (B:3:4)",
    allocations: 33
  }]
}, {
  time: 250,
  frames: [{
    location: "x (A:1:2)",
    allocations: 22
  }, {
    location: "y (B:3:4)",
    allocations: 33
  }, {
    location: "C:5:6",
    allocations: 44
  }]
}];
