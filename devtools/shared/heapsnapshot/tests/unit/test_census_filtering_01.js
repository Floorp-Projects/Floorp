/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test filtering basic CensusTreeNode trees.

function run_test() {
  const BREAKDOWN = {
    by: "coarseType",
    objects: {
      by: "objectClass",
      then: { by: "count", count: true, bytes: true },
      other: { by: "count", count: true, bytes: true },
    },
    scripts: {
      by: "internalType",
      then: { by: "count", count: true, bytes: true },
    },
    strings: {
      by: "internalType",
      then: { by: "count", count: true, bytes: true },
    },
    other:{
      by: "internalType",
      then: { by: "count", count: true, bytes: true },
    },
  };

  const REPORT = {
    objects: {
      Array: { bytes: 50, count: 5 },
      UInt8Array: { bytes: 80, count: 8 },
      Int32Array: { bytes: 320, count: 32 },
      other: { bytes: 0, count: 0 },
    },
    scripts: {
      "js::jit::JitScript": { bytes: 30, count: 3 },
    },
    strings: {
      JSAtom: { bytes: 60, count: 6 },
    },
    other: {
      "js::Shape": { bytes: 80, count: 8 },
    }
  };

  const EXPECTED = {
    name: null,
    bytes: 0,
    totalBytes: 620,
    count: 0,
    totalCount: 62,
    children: [
      {
        name: "objects",
        bytes: 0,
        totalBytes: 450,
        count: 0,
        totalCount: 45,
        children: [
          {
            name: "Int32Array",
            bytes: 320,
            totalBytes: 320,
            count: 32,
            totalCount: 32,
            children: undefined,
            id: 15,
            parent: 14,
            reportLeafIndex: 4,
          },
          {
            name: "UInt8Array",
            bytes: 80,
            totalBytes: 80,
            count: 8,
            totalCount: 8,
            children: undefined,
            id: 16,
            parent: 14,
            reportLeafIndex: 3,
          },
          {
            name: "Array",
            bytes: 50,
            totalBytes: 50,
            count: 5,
            totalCount: 5,
            children: undefined,
            id: 17,
            parent: 14,
            reportLeafIndex: 2,
          }
        ],
        id: 14,
        parent: 13,
        reportLeafIndex: undefined,
      }
    ],
    id: 13,
    parent: undefined,
    reportLeafIndex: undefined,
  };

  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED, { filter: "Array" });
}
