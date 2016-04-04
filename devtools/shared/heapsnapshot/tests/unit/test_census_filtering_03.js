/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test filtering with no matches.

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
    children: undefined,
    id: 13,
    parent: undefined,
    reportLeafIndex: undefined,
  };

  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED, { filter: "zzzzzzzzzzzzzzzzzzzz" });
}
