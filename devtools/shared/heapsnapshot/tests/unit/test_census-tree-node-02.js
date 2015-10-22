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
  bytes: 0,
  totalBytes: 111,
  count: 0,
  totalCount: 12,
  children: [
    {
      name: "other",
      count: 0,
      totalCount: 7,
      bytes: 0,
      totalBytes: 70,
      children: [
        {
          name: "js::Shape2",
          bytes: 40,
          totalBytes: 40,
          count: 4,
          totalCount: 4,
          children: undefined
        },
        {
          name: "js::Shape",
          bytes: 30,
          totalBytes: 30,
          count: 3,
          totalCount: 3,
          children: undefined
        },
      ]
    },
    {
      name: "objects",
      count: 0,
      totalCount: 3,
      bytes: 0,
      totalBytes: 30,
      children: [
        {
          name: "Array",
          bytes: 20,
          totalBytes: 20,
          count: 2,
          totalCount: 2,
          children: undefined
        },
        {
          name: "Function",
          bytes: 10,
          totalBytes: 10,
          count: 1,
          totalCount: 1,
          children: undefined
        },
      ]
    },
    {
      name: "strings",
      count: 1,
      totalCount: 1,
      bytes: 10,
      totalBytes: 10,
      children: undefined
    },
    {
      name: "scripts",
      count: 1,
      totalCount: 1,
      bytes: 1,
      totalBytes: 1,
      children: undefined
    },
  ]
};

function run_test() {
  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED);
}
