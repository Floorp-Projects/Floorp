/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test that repeatedly converting the same census report to a CensusTreeNode
 * tree results in the same CensusTreeNode tree.
 */

function run_test() {
  const BREAKDOWN = {
    by: "filename",
    then: {
      by: "internalType",
      then: { by: "count", count: true, bytes: true }
    },
    noFilename: {
      by: "internalType",
      then: { by: "count", count: true, bytes: true }
    },
  };

  const REPORT = {
    "http://example.com/app.js": {
      JSScript: { count: 10, bytes: 100 }
    },
    "http://example.com/ads.js": {
      "js::LazyScript": { count: 20, bytes: 200 }
    },
    "http://example.com/trackers.js": {
      JSScript: { count: 30, bytes: 300 }
    },
    noFilename: {
      "js::jit::JitCode": { count: 40, bytes: 400 }
    }
  };

  const first = censusReportToCensusTreeNode(BREAKDOWN, REPORT);
  const second = censusReportToCensusTreeNode(BREAKDOWN, REPORT);
  const third = censusReportToCensusTreeNode(BREAKDOWN, REPORT);

  assertStructurallyEquivalent(first, second);
  assertStructurallyEquivalent(second, third);
}
