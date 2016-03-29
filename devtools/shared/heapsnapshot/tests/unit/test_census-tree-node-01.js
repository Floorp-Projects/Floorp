/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests CensusTreeNode with `internalType` breakdown.
 */

const BREAKDOWN = {
  by: "internalType",
  then: { by: "count", count: true, bytes: true }
};

const REPORT = {
  "JSObject": {
    "bytes": 100,
    "count": 10,
  },
  "js::Shape": {
    "bytes": 500,
    "count": 50,
  },
  "JSString": {
    "bytes": 10,
    "count": 1,
  },
};

const EXPECTED = {
  name: null,
  bytes: 0,
  totalBytes: 610,
  count: 0,
  totalCount: 61,
  children: [
    {
      name: "js::Shape",
      bytes: 500,
      totalBytes: 500,
      count: 50,
      totalCount: 50,
      children: undefined,
      id: 3,
      parent: 1,
      reportLeafIndex: 2,
    },
    {
      name: "JSObject",
      bytes: 100,
      totalBytes: 100,
      count: 10,
      totalCount: 10,
      children: undefined,
      id: 2,
      parent: 1,
      reportLeafIndex: 1,
    },
    {
      name: "JSString",
      bytes: 10,
      totalBytes: 10,
      count: 1,
      totalCount: 1,
      children: undefined,
      id: 4,
      parent: 1,
      reportLeafIndex: 3,
    },
  ],
  id: 1,
  parent: undefined,
  reportLeafIndex: undefined,
};

function run_test() {
  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED);
}
