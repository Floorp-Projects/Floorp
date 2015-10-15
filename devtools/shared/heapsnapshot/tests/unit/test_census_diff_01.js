/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test diffing census reports of breakdown by "internalType".

const BREAKDOWN = {
  by: "internalType",
  then: { by: "count", count: true, bytes: true }
};

const REPORT1 = {
  "JSObject": {
    "count": 10,
    "bytes": 100,
  },
  "js::Shape": {
    "count": 50,
    "bytes": 500,
  },
  "JSString": {
    "count": 0,
    "bytes": 0,
  },
  "js::LazyScript": {
    "count": 1,
    "bytes": 10,
  },
};

const REPORT2 = {
  "JSObject": {
    "count": 11,
    "bytes": 110,
  },
  "js::Shape": {
    "count": 51,
    "bytes": 510,
  },
  "JSString": {
    "count": 1,
    "bytes": 1,
  },
  "js::BaseShape": {
    "count": 1,
    "bytes": 42,
  },
};

const EXPECTED = {
  "JSObject": {
    "count": 1,
    "bytes": 10,
  },
  "js::Shape": {
    "count": 1,
    "bytes": 10,
  },
  "JSString": {
    "count": 1,
    "bytes": 1,
  },
  "js::LazyScript": {
    "count": -1,
    "bytes": -10,
  },
  "js::BaseShape": {
    "count": 1,
    "bytes": 42,
  },
};

function run_test() {
  assertDiff(BREAKDOWN, REPORT1, REPORT2, EXPECTED);
}
