/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests CensusTreeNode with `coarseType` breakdown.
 */

function run_test() {
  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED, `${JSON.stringify(BREAKDOWN)} has correct results.`);
}

const countBreakdown = { by: "count", count: true, bytes: true };

const BREAKDOWN = {
  by: "coarseType",
  objects: { by: "objectClass", then: countBreakdown },
  strings: countBreakdown,
  other: { by: "internalType", then: countBreakdown },
};

const REPORT = {
  "objects": {
    "Function": { bytes: 10, count: 1 },
    "Array": { bytes: 20, count: 2 },
  },
  "strings": { bytes: 10, count: 1 },
  "other": {
    "js::Shape": { bytes: 30, count: 3 },
    "js::Shape2": { bytes: 40, count: 4 }
  },
};

const EXPECTED = {
  children: [
    { name: "strings", bytes: 10, count: 1, },
    { name: "objects", children: [
      { name: "Array", bytes: 20, count: 2, },
      { name: "Function", bytes: 10, count: 1, },
    ]},
    { name: "other", children: [
      { name: "js::Shape2", bytes: 40, count: 4, },
      { name: "js::Shape", bytes: 30, count: 3, },
    ]},
  ]
};
