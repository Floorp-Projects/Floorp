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
    other: { count: 0, bytes: 0 }
  },
  strings: {
    "JSString": { count: 1, bytes: 42 },
  },
  scripts: {},
  other: {}
};

const expected = [
  "strings",
  "JSString"
];

const shallowSize = 42;

function run_test() {
  assertLabelAndShallowSize(breakdown, description, shallowSize, expected);
}
