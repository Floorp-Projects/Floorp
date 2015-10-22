/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests CensusTreeNode with `allocationStack` => `objectClass` breakdown.
 */

function run_test() {
  const countBreakdown = { by: "count", count: true, bytes: true };

  const BREAKDOWN = {
    by: "allocationStack",
    then: {
      by: "objectClass",
      then: countBreakdown,
      other: countBreakdown
    },
    noStack: countBreakdown,
  };

  let stack;

  (function a() {
    (function b() {
      (function c() {
        stack = saveStack(3);
      }());
    }());
  }());

  const REPORT = new Map([
    [stack,     { Foo:   { bytes: 10, count: 1 },
                  Bar:   { bytes: 20, count: 2 },
                  Baz:   { bytes: 30, count: 3 },
                  other: { bytes: 40, count: 4 }
                }],
    ["noStack", { bytes: 50, count: 5 }],
  ]);

  const EXPECTED = {
    name: null,
    bytes: undefined,
    count: undefined,
    children: [
      {
        name: "noStack",
        bytes: 50,
        count: 5,
        children: undefined
      },
      {
        name: stack.parent.parent,
        bytes: undefined,
        count: undefined,
        children: [
          {
            name: stack.parent,
            bytes: undefined,
            count: undefined,
            children: [
              {
                name: stack,
                bytes: undefined,
                count: undefined,
                children: [
                  {
                    name: "other",
                    bytes: 40,
                    count: 4,
                    children: undefined
                  },
                  {
                    name: "Baz",
                    bytes: 30,
                    count: 3,
                    children: undefined
                  },
                  {
                    name: "Bar",
                    bytes: 20,
                    count: 2,
                    children: undefined
                  },
                  {
                    name: "Foo",
                    bytes: 10,
                    count: 1,
                    children: undefined
                  },
                ]
              }
            ]
          }
        ]
      }
    ]
  };

  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED);
}
