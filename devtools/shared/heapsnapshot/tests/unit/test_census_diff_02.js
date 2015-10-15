/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test diffing census reports of breakdown by "count".

const BREAKDOWN = { by: "count", count: true, bytes: true };

const REPORT1 = {
  "count": 10,
  "bytes": 100,
};

const REPORT2 = {
  "count": 11,
  "bytes": 110,
};

const EXPECTED = {
  "count": 1,
  "bytes": 10,
};

function run_test() {
  assertDiff(BREAKDOWN, REPORT1, REPORT2, EXPECTED);
}
