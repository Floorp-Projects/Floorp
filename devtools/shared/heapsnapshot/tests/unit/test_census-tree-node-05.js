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
    [stack, { Foo:   { bytes: 10, count: 1 },
                  Bar:   { bytes: 20, count: 2 },
                  Baz:   { bytes: 30, count: 3 },
                  other: { bytes: 40, count: 4 }
                }],
    ["noStack", { bytes: 50, count: 5 }],
  ]);

  const EXPECTED = {
    name: null,
    bytes: 0,
    totalBytes: 150,
    count: 0,
    totalCount: 15,
    children: [
      {
        name: stack.parent.parent,
        bytes: 0,
        totalBytes: 100,
        count: 0,
        totalCount: 10,
        children: [
          {
            name: stack.parent,
            bytes: 0,
            totalBytes: 100,
            count: 0,
            totalCount: 10,
            children: [
              {
                name: stack,
                bytes: 0,
                totalBytes: 100,
                count: 0,
                totalCount: 10,
                children: [
                  {
                    name: "other",
                    bytes: 40,
                    totalBytes: 40,
                    count: 4,
                    totalCount: 4,
                    children: undefined,
                    id: 8,
                    parent: 4,
                    reportLeafIndex: 5,
                  },
                  {
                    name: "Baz",
                    bytes: 30,
                    totalBytes: 30,
                    count: 3,
                    totalCount: 3,
                    children: undefined,
                    id: 7,
                    parent: 4,
                    reportLeafIndex: 4,
                  },
                  {
                    name: "Bar",
                    bytes: 20,
                    totalBytes: 20,
                    count: 2,
                    totalCount: 2,
                    children: undefined,
                    id: 6,
                    parent: 4,
                    reportLeafIndex: 3,
                  },
                  {
                    name: "Foo",
                    bytes: 10,
                    totalBytes: 10,
                    count: 1,
                    totalCount: 1,
                    children: undefined,
                    id: 5,
                    parent: 4,
                    reportLeafIndex: 2,
                  },
                ],
                id: 4,
                parent: 3,
                reportLeafIndex: undefined,
              }
            ],
            id: 3,
            parent: 2,
            reportLeafIndex: undefined,
          }
        ],
        id: 2,
        parent: 1,
        reportLeafIndex: undefined,
      },
      {
        name: "noStack",
        bytes: 50,
        totalBytes: 50,
        count: 5,
        totalCount: 5,
        children: undefined,
        id: 9,
        parent: 1,
        reportLeafIndex: 6,
      },
    ],
    id: 1,
    parent: undefined,
    reportLeafIndex: undefined,
  };

  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED);
}
