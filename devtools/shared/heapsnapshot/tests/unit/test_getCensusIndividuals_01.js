/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test basic functionality of `CensusUtils.getCensusIndividuals`.

function run_test() {
  const stack1 = saveStack(1);
  const stack2 = saveStack(1);
  const stack3 = saveStack(1);

  const COUNT = { by: "count", count: true, bytes: true };
  const INTERNAL_TYPE = { by: "internalType", then: COUNT };

  const BREAKDOWN = {
    by: "allocationStack",
    then: INTERNAL_TYPE,
    noStack: INTERNAL_TYPE,
  };

  const MOCK_SNAPSHOT = {
    takeCensus: ({ breakdown }) => {
      assertStructurallyEquivalent(
        breakdown,
        CensusUtils.countToBucketBreakdown(BREAKDOWN));

      //                                DFS Index
      return new Map([               // 0
        [stack1, {                   // 1
          JSObject: [101, 102, 103], // 2
          JSString: [111, 112, 113], // 3
        }],
        [stack2, {                   // 4
          JSObject: [201, 202, 203], // 5
          JSString: [211, 212, 213], // 6
        }],
        [stack3, {                   // 7
          JSObject: [301, 302, 303], // 8
          JSString: [311, 312, 313], // 9
        }],
        ["noStack", {                // 10
          JSObject: [401, 402, 403], // 11
          JSString: [411, 412, 413], // 12
        }],
      ]);
    }
  };

  const INDICES = new Set([3, 5, 9]);

  const EXPECTED = new Set([111, 112, 113,
                            201, 202, 203,
                            311, 312, 313]);

  const actual = new Set(CensusUtils.getCensusIndividuals(INDICES,
                                                          BREAKDOWN,
                                                          MOCK_SNAPSHOT));

  assertStructurallyEquivalent(EXPECTED, actual);
}
