/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can generate label structures from node description reports.

const breakdown = {
  by: "coarseType",
  objects: {
    by: "objectClass",
    then: {
      by: "allocationStack",
      then: { by: "count", count: true, bytes: true },
      noStack: { by: "count", count: true, bytes: true },
    },
    other: { by: "count", count: true, bytes: true },
  },
  strings: {
    by: "internalType",
    then: { by: "count", count: true, bytes: true },
  },
  scripts: {
    by: "internalType",
    then: { by: "count", count: true, bytes: true },
  },
  other: {
    by: "internalType",
    then: { by: "count", count: true, bytes: true },
  },
};

const stack = saveStack();

const description = {
  objects: {
    Array: new Map([[stack, { count: 1, bytes: 512 }]]),
    other: { count: 0, bytes: 0 }
  },
  strings: {},
  scripts: {},
  other: {}
};

const expected = [
  "objects",
  "Array",
  stack
];

const shallowSize = 512;

function run_test() {
  assertLabelAndShallowSize(breakdown, description, shallowSize, expected);
}
