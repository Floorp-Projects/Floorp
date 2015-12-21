/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can generate label structures from node description reports.

const breakdown = {
  by: "coarseType",
  objects: {
    by: "objectClass",
    then: { by: "count", count: true, bytes: true },
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

const description = {
  objects: {
    other: { count: 1, bytes: 10 }
  },
  strings: {},
  scripts: {},
  other: {}
};

const expected = [
  "objects",
  "other"
];

const shallowSize = 10;

function run_test() {
  assertLabelAndShallowSize(breakdown, description, shallowSize, expected);
}
