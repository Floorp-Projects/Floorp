/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that when we take a census and get a bucket list of ids that matched the
// given category, that the returned ids are all in the snapshot and their
// reported category.

function run_test() {
  const g = newGlobal();
  const dbg = new Debugger(g);

  const path = saveNewHeapSnapshot({ debugger: dbg });
  const snapshot = readHeapSnapshot(path);

  const bucket = { by: "bucket" };
  const count = { by: "count", count: true, bytes: false };
  const objectClassCount = { by: "objectClass", then: count, other: count };

  const byClassName = snapshot.takeCensus({
    breakdown: {
      by: "objectClass",
      then: bucket,
      other: bucket,
    }
  });

  const byClassNameCount = snapshot.takeCensus({
    breakdown: objectClassCount
  });

  const keys = new Set(Object.keys(byClassName));
  equal(keys.size, Object.keys(byClassNameCount).length,
        "Should have the same number of keys.");
  for (let k of Object.keys(byClassNameCount)) {
    ok(keys.has(k), "Should not have any unexpected class names");
  }

  for (let key of Object.keys(byClassName)) {
    equal(byClassNameCount[key].count, byClassName[key].length,
          "Length of the bucket and count should be equal");

    for (let id of byClassName[key]) {
      const desc = snapshot.describeNode(objectClassCount, id);
      equal(desc[key].count, 1,
            "Describing the bucketed node confirms that it belongs to the category");
    }
  }

  do_test_finished();
}
