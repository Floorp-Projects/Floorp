/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests CensusTreeNode with `coarseType` breakdown.
 */

const countBreakdown = { by: "count", count: true, bytes: true };

const BREAKDOWN = {
  by: "coarseType",
  objects: { by: "objectClass", then: countBreakdown },
  strings: countBreakdown,
  scripts: countBreakdown,
  other: { by: "internalType", then: countBreakdown },
};

const REPORT = {
  "objects": {
    "Function": { bytes: 10, count: 1 },
    "Array": { bytes: 20, count: 2 },
  },
  "strings": { bytes: 10, count: 1 },
  "scripts": { bytes: 1, count: 1 },
  "other": {
    "js::Shape": { bytes: 30, count: 3 },
    "js::Shape2": { bytes: 40, count: 4 }
  },
};

const EXPECTED = {
  name: null,
  bytes: undefined,
  count: undefined,
  children: [
    {
      name: "strings",
      count: 1,
      bytes: 10,
      children: undefined
    },
    {
      name: "scripts",
      count: 1,
      bytes: 1,
      children: undefined
    },
    {
      name: "objects",
      count: undefined,
      bytes: undefined,
      children: [
        { name: "Array", bytes: 20, count: 2, children: undefined },
        { name: "Function", bytes: 10, count: 1, children: undefined },
      ]
    },
    {
      name: "other",
      count: undefined,
      bytes: undefined,
      children: [
        { name: "js::Shape2", bytes: 40, count: 4, children: undefined },
        { name: "js::Shape", bytes: 30, count: 3, children: undefined },
      ]
    },
  ]
};

function run_test() {
  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED);
}
