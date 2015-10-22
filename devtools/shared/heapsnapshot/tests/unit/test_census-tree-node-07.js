/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test inverting CensusTreeNode with a non-allocation stack breakdown.
 */

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
    totalBytes: 0,
    count: 0,
    totalCount: 0,
    children: [
      {
        name: "js::Shape",
        bytes: 80,
        totalBytes: 80,
        count: 8,
        totalCount: 8,
        children: [
          {
            name: "other",
            bytes: 0,
            totalBytes: 80,
            count: 0,
            totalCount: 8,
            children: [
              {
                name: null,
                bytes: 0,
                totalBytes: 220,
                count: 0,
                totalCount: 22,
                children: undefined
              }
            ]
          }
        ]
      },
      {
        name: "JSAtom",
        bytes: 60,
        totalBytes: 60,
        count: 6,
        totalCount: 6,
        children: [
          {
            name: "strings",
            bytes: 0,
            totalBytes: 60,
            count: 0,
            totalCount: 6,
            children: [
              {
                name: null,
                bytes: 0,
                totalBytes: 220,
                count: 0,
                totalCount: 22,
                children: undefined
              }
            ]
          }
        ]
      },
      {
        name: "Array",
        bytes: 50,
        totalBytes: 50,
        count: 5,
        totalCount: 5,
        children: [
          {
            name: "objects",
            bytes: 0,
            totalBytes: 50,
            count: 0,
            totalCount: 5,
            children: [
              {
                name: null,
                bytes: 0,
                totalBytes: 220,
                count: 0,
                totalCount: 22,
                children: undefined
              }
            ]
          }
        ]
      },
      {
        name: "js::jit::JitScript",
        bytes: 30,
        totalBytes: 30,
        count: 3,
        totalCount: 3,
        children: [
          {
            name: "scripts",
            bytes: 0,
            totalBytes: 30,
            count: 0,
            totalCount: 3,
            children: [
              {
                name: null,
                bytes: 0,
                totalBytes: 220,
                count: 0,
                totalCount: 22,
                children: undefined
              }
            ]
          }
        ]
      },
      {
        name: "other",
        bytes: 0,
        totalBytes: 0,
        count: 0,
        totalCount: 0,
        children: [
          {
            name: "objects",
            bytes: 0,
            totalBytes: 50,
            count: 0,
            totalCount: 5,
            children: [
              {
                name: null,
                bytes: 0,
                totalBytes: 220,
                count: 0,
                totalCount: 22,
                children: undefined
              }
            ]
          }
        ]
      }
    ]
  };

  compareCensusViewData(BREAKDOWN, REPORT, EXPECTED, { invert: true });
}
