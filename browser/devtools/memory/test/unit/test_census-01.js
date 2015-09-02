/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests CensusTreeNode with `internalType` breakdown.
 */
function run_test() {
  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED, `${JSON.stringify(BREAKDOWN)} has correct results.`);
}

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
  children: [
    { name: "js::Shape", bytes: 500, count: 50, },
    { name: "JSObject", bytes: 100, count: 10, },
    { name: "JSString", bytes: 0, count: 0, },
  ],
};
