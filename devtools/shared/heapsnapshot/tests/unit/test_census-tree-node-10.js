/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test when multiple leaves in the census report map to the same node in an
 * inverted CensusReportTree.
 */

function run_test() {
  const BREAKDOWN = {
    by: "coarseType",
    objects: {
      by: "objectClass",
      then: { by: "count", count: true, bytes: true },
    },
    other: {
      by: "internalType",
      then: { by: "count", count: true, bytes: true },
    },
    strings: { by: "count", count: true, bytes: true },
    scripts: { by: "count", count: true, bytes: true },
  };

  const REPORT = {
    objects: {
      Array: { count: 1, bytes: 10 },
    },
    other: {
      Array: { count: 1, bytes: 10 },
    },
    strings: { count: 0, bytes: 0 },
    scripts: { count: 0, bytes: 0 },
  };

  const node = censusReportToCensusTreeNode(BREAKDOWN, REPORT, { invert: true });

  equal(node.children[0].name, "Array");
  equal(node.children[0].reportLeafIndex.size, 2);
  dumpn(`node.children[0].reportLeafIndex = ${[...node.children[0].reportLeafIndex]}`);
  ok(node.children[0].reportLeafIndex.has(2));
  ok(node.children[0].reportLeafIndex.has(6));
}
