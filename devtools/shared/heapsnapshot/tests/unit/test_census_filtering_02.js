/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test filtering CensusTreeNode trees with an `allocationStack` breakdown.

function run_test() {
  const countBreakdown = { by: "count", count: true, bytes: true };

  const BREAKDOWN = {
    by: "allocationStack",
    then: countBreakdown,
    noStack: countBreakdown,
  };

  let stack1, stack2, stack3, stack4;

  (function foo() {
    (function bar() {
      (function baz() {
        stack1 = saveStack(3);
      }());
      (function quux() {
        stack2 = saveStack(3);
        stack3 = saveStack(3);
      }());
    }());
    stack4 = saveStack(2);
  }());

  const stack5 = saveStack(1);

  const REPORT = new Map([
    [stack1, { bytes: 10, count: 1 }],
    [stack2, { bytes: 20, count: 2 }],
    [stack3, { bytes: 30, count: 3 }],
    [stack4, { bytes: 40, count: 4 }],
    [stack5, { bytes: 50, count: 5 }],
    ["noStack", { bytes: 60, count: 6 }],
  ]);

  const EXPECTED = {
    name: null,
    bytes: 0,
    totalBytes: 210,
    count: 0,
    totalCount: 21,
    children: [
      {
        name: stack1.parent.parent,
        bytes: 0,
        totalBytes: 60,
        count: 0,
        totalCount: 6,
        children: [
          {
            name: stack2.parent,
            bytes: 0,
            totalBytes: 50,
            count: 0,
            totalCount: 5,
            children: [
              {
                name: stack3,
                bytes: 30,
                totalBytes: 30,
                count: 3,
                totalCount: 3,
                children: undefined,
                id: 15,
                parent: 14,
                reportLeafIndex: 3,
              },
              {
                name: stack2,
                bytes: 20,
                totalBytes: 20,
                count: 2,
                totalCount: 2,
                children: undefined,
                id: 16,
                parent: 14,
                reportLeafIndex: 2,
              }
            ],
            id: 14,
            parent: 13,
            reportLeafIndex: undefined,
          },
          {
            name: stack1.parent,
            bytes: 0,
            totalBytes: 10,
            count: 0,
            totalCount: 1,
            children: [
              {
                name: stack1,
                bytes: 10,
                totalBytes: 10,
                count: 1,
                totalCount: 1,
                children: undefined,
                id: 18,
                parent: 17,
                reportLeafIndex: 1,
              }
            ],
            id: 17,
            parent: 13,
            reportLeafIndex: undefined,
          }
        ],
        id: 13,
        parent: 12,
        reportLeafIndex: undefined,
      }
    ],
    id: 12,
    parent: undefined,
    reportLeafIndex: undefined,
  };

  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED, { filter: "bar" });
}
