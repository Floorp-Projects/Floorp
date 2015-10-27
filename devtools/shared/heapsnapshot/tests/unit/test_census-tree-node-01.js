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
    "bytes": 0,
    "count": 0,
  },
};

const EXPECTED = {
  name: null,
  bytes: 0,
  totalBytes: 600,
  count: 0,
  totalCount: 60,
  children: [
    {
      name: "js::Shape",
      bytes: 500,
      totalBytes: 500,
      count: 50,
      totalCount: 50,
      children: undefined,
      id: 3,
      parent: 1
    },
    {
      name: "JSObject",
      bytes: 100,
      totalBytes: 100,
      count: 10,
      totalCount: 10,
      children: undefined,
      id: 2,
      parent: 1
    },
    {
      name: "JSString",
      bytes: 0,
      totalBytes: 0,
      count: 0,
      totalCount: 0,
      children: undefined,
      id: 4,
      parent: 1
    },
  ],
  id: 1,
  parent: undefined,
};

function run_test() {
  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED);
}
