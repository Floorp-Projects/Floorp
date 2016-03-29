/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests CensusTreeNode with `objectClass` breakdown.
 */

const countBreakdown = { by: "count", count: true, bytes: true };

const BREAKDOWN = {
  by: "objectClass",
  then: countBreakdown,
  other: { by: "internalType", then: countBreakdown }
};

const REPORT = {
  "Function": { bytes: 10, count: 10 },
  "Array": { bytes: 100, count: 1 },
  "other": {
    "JIT::CODE::NOW!!!": { bytes: 20, count: 2 },
    "JIT::CODE::LATER!!!": { bytes: 40, count: 4 }
  }
};

const EXPECTED = {
  name: null,
  count: 0,
  totalCount: 17,
  bytes: 0,
  totalBytes: 170,
  children: [
    {
      name: "Array",
      bytes: 100,
      totalBytes: 100,
      count: 1,
      totalCount: 1,
      children: undefined,
      id: 3,
      parent: 1,
      reportLeafIndex: 2,
    },
    {
      name: "other",
      count: 0,
      totalCount: 6,
      bytes: 0,
      totalBytes: 60,
      children: [
        {
          name: "JIT::CODE::LATER!!!",
          bytes: 40,
          totalBytes: 40,
          count: 4,
          totalCount: 4,
          children: undefined,
          id: 6,
          parent: 4,
          reportLeafIndex: 5,
        },
        {
          name: "JIT::CODE::NOW!!!",
          bytes: 20,
          totalBytes: 20,
          count: 2,
          totalCount: 2,
          children: undefined,
          id: 5,
          parent: 4,
          reportLeafIndex: 4,
        },
      ],
      id: 4,
      parent: 1,
      reportLeafIndex: undefined,
    },
    {
      name: "Function",
      bytes: 10,
      totalBytes: 10,
      count: 10,
      totalCount: 10,
      children: undefined,
      id: 2,
      parent: 1,
      reportLeafIndex: 1,
    },
  ],
  id: 1,
  parent: undefined,
  reportLeafIndex: undefined,
};

function run_test() {
  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED);
}
