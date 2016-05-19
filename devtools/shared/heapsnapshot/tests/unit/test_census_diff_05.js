/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test diffing census reports of breakdown by "allocationStack".

const BREAKDOWN = {
  by: "allocationStack",
  then: { by: "count", count: true, bytes: true },
  noStack: { by: "count", count: true, bytes: true },
};

const stack1 = saveStack();
const stack2 = saveStack();
const stack3 = saveStack();

const REPORT1 = new Map([
  [stack1, { "count": 10, "bytes": 100 }],
  [stack2, { "count": 1, "bytes": 10 }],
]);

const REPORT2 = new Map([
  [stack2, { "count": 10, "bytes": 100 }],
  [stack3, { "count": 1, "bytes": 10 }],
]);

const EXPECTED = new Map([
  [stack1, { "count": -10, "bytes": -100 }],
  [stack2, { "count": 9, "bytes": 90 }],
  [stack3, { "count": 1, "bytes": 10 }],
]);

function run_test() {
  assertDiff(BREAKDOWN, REPORT1, REPORT2, EXPECTED);
}
