/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test diffing census reports of breakdown by "objectClass".

const BREAKDOWN = {
  by: "objectClass",
  then: { by: "count", count: true, bytes: true },
  other: { by: "count", count: true, bytes: true },
};

const REPORT1 = {
  "Array": {
    count: 1,
    bytes: 100,
  },
  "Function": {
    count: 10,
    bytes: 10,
  },
  "other": {
    count: 10,
    bytes: 100,
  }
};

const REPORT2 = {
  "Object": {
    count: 1,
    bytes: 100,
  },
  "Function": {
    count: 20,
    bytes: 20,
  },
  "other": {
    count: 10,
    bytes: 100,
  }
};

const EXPECTED = {
  "Array": {
    count: -1,
    bytes: -100,
  },
  "Function": {
    count: 10,
    bytes: 10,
  },
  "other": {
    count: 0,
    bytes: 0,
  },
  "Object": {
    count: 1,
    bytes: 100,
  },
};

function run_test() {
  assertDiff(BREAKDOWN, REPORT1, REPORT2, EXPECTED);
}
