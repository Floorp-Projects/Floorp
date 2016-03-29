/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test basic functionality of `CensusUtils.getReportLeaves`.

function run_test() {
  const BREAKDOWN = {
    by: "coarseType",
    objects: {
      by: "objectClass",
      then: { by: "count", count: true, bytes: true },
      other: { by: "count", count: true, bytes: true },
    },
    strings: { by: "count", count: true, bytes: true },
    scripts: {
      by: "filename",
      then: {
        by: "internalType",
        then: { by: "count", count: true, bytes: true },
      },
      noFilename: {
        by: "internalType",
        then: { by: "count", count: true, bytes: true },
      },
    },
    other: {
      by: "internalType",
      then: { by: "count", count: true, bytes: true },
    },
  };

  const REPORT = {
    objects: {
      Array: { count: 6, bytes: 60 },
      Function: { count: 1, bytes: 10 },
      Object: { count: 1, bytes: 10 },
      RegExp: { count: 1, bytes: 10 },
      other: { count: 0, bytes: 0 },
    },
    strings: { count: 1, bytes: 10 },
    scripts: {
      "foo.js": {
        JSScript: { count: 1, bytes: 10 },
        "js::jit::IonScript": { count: 1, bytes: 10 },
      },
      noFilename: {
        JSScript: { count: 1, bytes: 10 },
        "js::jit::IonScript": { count: 1, bytes: 10 },
      },
    },
    other: {
      "js::Shape": { count: 7, bytes: 70 },
      "js::BaseShape": { count: 1, bytes: 10 },
    },
  };

  const root = censusReportToCensusTreeNode(BREAKDOWN, REPORT);
  dumpn("CensusTreeNode tree = " + JSON.stringify(root, null, 4));

  (function assertEveryNodeCanFindItsLeaf(node) {
    if (node.reportLeafIndex) {
      const [ leaf ] = CensusUtils.getReportLeaves(new Set([node.reportLeafIndex]),
                                                   BREAKDOWN,
                                                   REPORT);
      ok(leaf, "Should be able to find leaf for a node with a reportLeafIndex = " + node.reportLeafIndex);
    }

    if (node.children) {
      for (let child of node.children) {
        assertEveryNodeCanFindItsLeaf(child);
      }
    }
  }(root));

  // Test finding multiple leaves at a time.

  function find(name, node) {
    if (node.name === name) {
      return node;
    }

    if (node.children) {
      for (let child of node.children) {
        const found = find(name, child);
        if (found) {
          return found;
        }
      }
    }
  }

  const arrayNode = find("Array", root);
  ok(arrayNode);
  equal(typeof arrayNode.reportLeafIndex, "number");

  const shapeNode = find("js::Shape", root);
  ok(shapeNode);
  equal(typeof shapeNode.reportLeafIndex, "number");

  const indices = new Set([arrayNode.reportLeafIndex, shapeNode.reportLeafIndex]);
  const leaves = CensusUtils.getReportLeaves(indices, BREAKDOWN, REPORT);
  equal(leaves.length, 2);

  // `getReportLeaves` does not guarantee order of the results, so handle both
  // cases.
  ok(leaves.some(l => l === REPORT.objects.Array));
  ok(leaves.some(l => l === REPORT.other["js::Shape"]));

  // Test that bad indices do not yield results.

  const none = CensusUtils.getReportLeaves(new Set([999999999999]), BREAKDOWN, REPORT);
  equal(none.length, 0);
}
