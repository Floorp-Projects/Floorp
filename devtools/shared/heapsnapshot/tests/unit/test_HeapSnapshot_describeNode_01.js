/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can describe nodes with a breakdown.

function run_test() {
  const path = saveNewHeapSnapshot();
  const snapshot = ChromeUtils.readHeapSnapshot(path);
  ok(snapshot.describeNode);
  equal(typeof snapshot.describeNode, "function");

  const dt = snapshot.computeDominatorTree();

  let threw = false;
  try {
    snapshot.describeNode(undefined, dt.root);
  } catch (_) {
    threw = true;
  }
  ok(threw, "Should require a breakdown");

  const breakdown = {
    by: "coarseType",
    objects: { by: "objectClass" },
    scripts: { by: "internalType" },
    strings: { by: "internalType" },
    other: { by: "internalType" }
  };

  threw = false;
  try {
    snapshot.describeNode(breakdown, 0);
  } catch (_) {
    threw = true;
  }
  ok(threw, "Should throw when given an invalid node id");

  const description = snapshot.describeNode(breakdown, dt.root);
  ok(description);
  ok(description.other);
  ok(description.other["JS::ubi::RootList"]);
}
