/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test inverting CensusTreeNode with a non-allocation stack breakdown.
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

  const EXPECTED = {
    name: null,
    bytes: 0,
    totalBytes: 1000,
    count: 0,
    totalCount: 100,
    children: [
      {
        name: "noFilename",
        bytes: 0,
        totalBytes: 400,
        count: 0,
        totalCount: 40,
        children: [
          {
            name: "js::jit::JitCode",
            bytes: 400,
            totalBytes: 400,
            count: 40,
            totalCount: 40,
            children: undefined,
            id: 9,
            parent: 8,
            reportLeafIndex: 8,
          }
        ],
        id: 8,
        parent: 1,
        reportLeafIndex: undefined,
      },
      {
        name: "http://example.com/trackers.js",
        bytes: 0,
        totalBytes: 300,
        count: 0,
        totalCount: 30,
        children: [
          {
            name: "JSScript",
            bytes: 300,
            totalBytes: 300,
            count: 30,
            totalCount: 30,
            children: undefined,
            id: 7,
            parent: 6,
            reportLeafIndex: 6,
          }
        ],
        id: 6,
        parent: 1,
        reportLeafIndex: undefined,
      },
      {
        name: "http://example.com/ads.js",
        bytes: 0,
        totalBytes: 200,
        count: 0,
        totalCount: 20,
        children: [
          {
            name: "js::LazyScript",
            bytes: 200,
            totalBytes: 200,
            count: 20,
            totalCount: 20,
            children: undefined,
            id: 5,
            parent: 4,
            reportLeafIndex: 4,
          }
        ],
        id: 4,
        parent: 1,
        reportLeafIndex: undefined,
      },
      {
        name: "http://example.com/app.js",
        bytes: 0,
        totalBytes: 100,
        count: 0,
        totalCount: 10,
        children: [
          {
            name: "JSScript",
            bytes: 100,
            totalBytes: 100,
            count: 10,
            totalCount: 10,
            children: undefined,
            id: 3,
            parent: 2,
            reportLeafIndex: 2,
          }
        ],
        id: 2,
        parent: 1,
        reportLeafIndex: undefined,
      }
    ],
    id: 1,
    parent: undefined,
    reportLeafIndex: undefined,
  };

  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED);
}
