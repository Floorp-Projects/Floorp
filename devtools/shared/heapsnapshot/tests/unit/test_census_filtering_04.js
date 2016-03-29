/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the filtered nodes' counts and bytes are the same as they were when
// unfiltered.

function run_test() {
  const COUNT = { by: "count", count: true, bytes: true };
  const INTERNAL_TYPE = { by: "internalType", then: COUNT };

  const BREAKDOWN = {
    by: "coarseType",
    objects: { by: "objectClass", then: COUNT, other: COUNT },
    strings: COUNT,
    scripts: {
      by: "filename",
      then: INTERNAL_TYPE,
      noFilename: INTERNAL_TYPE
    },
    other: INTERNAL_TYPE,
  };

  const REPORT = {
    objects: {
      Function: {
        count: 7,
        bytes: 70
      },
      Array: {
        count: 6,
        bytes: 60
      }
    },
    scripts: {
      "http://mozilla.github.io/pdf.js/build/pdf.js": {
        "js::LazyScript": {
          count: 4,
          bytes: 40
        },
      }
    },
    strings: {
      count: 2,
      bytes: 20
    },
    other: {
      "js::Shape": {
        count: 1,
        bytes: 10
      }
    }
  };

  const EXPECTED = {
    name: null,
    bytes: 0,
    totalBytes: 200,
    count: 0,
    totalCount: 20,
    parent: undefined,
    children: [
      {
        name: "objects",
        bytes: 0,
        totalBytes: 130,
        count: 0,
        totalCount: 13,
        children: [
          {
            name: "Function",
            bytes: 70,
            totalBytes: 70,
            count: 7,
            totalCount: 7,
            id: 13,
            parent: 12,
            children: undefined,
            reportLeafIndex: 2,
          },
          {
            name: "Array",
            bytes: 60,
            totalBytes: 60,
            count: 6,
            totalCount: 6,
            id: 14,
            parent: 12,
            children: undefined,
            reportLeafIndex: 3,
          },
        ],
        id: 12,
        parent: 11,
        reportLeafIndex: undefined,
      }
    ],
    id: 11,
    reportLeafIndex: undefined,
  };

  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED, { filter: "objects" });
}
