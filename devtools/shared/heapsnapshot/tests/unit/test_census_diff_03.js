/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test diffing census reports of breakdown by "coarseType".

const BREAKDOWN = {
  by: "coarseType",
  objects: { by: "count", count: true, bytes: true },
  scripts: { by: "count", count: true, bytes: true },
  strings: { by: "count", count: true, bytes: true },
  other:   { by: "count", count: true, bytes: true },
};

const REPORT1 = {
  objects: {
    count: 1,
    bytes: 10,
  },
  scripts: {
    count: 1,
    bytes: 10,
  },
  strings: {
    count: 1,
    bytes: 10,
  },
  other: {
    count: 3,
    bytes: 30,
  },
};

const REPORT2 = {
  objects: {
    count: 1,
    bytes: 10,
  },
  scripts: {
    count: 0,
    bytes: 0,
  },
  strings: {
    count: 2,
    bytes: 20,
  },
  other: {
    count: 4,
    bytes: 40,
  },
};

const EXPECTED = {
  objects: {
    count: 0,
    bytes: 0,
  },
  scripts: {
    count: -1,
    bytes: -10,
  },
  strings: {
    count: 1,
    bytes: 10,
  },
  other: {
    count: 1,
    bytes: 10,
  },
};

function run_test() {
  assertDiff(BREAKDOWN, REPORT1, REPORT2, EXPECTED);
}
