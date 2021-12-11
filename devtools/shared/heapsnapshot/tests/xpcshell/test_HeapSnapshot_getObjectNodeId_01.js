/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test ChromeUtils.getObjectNodeId()

function run_test() {
  // Create a test object, which we want to analyse
  const testObject = {
    foo: {
      bar: {},
    },
  };

  const path = ChromeUtils.saveHeapSnapshot({ runtime: true });
  const snapshot = ChromeUtils.readHeapSnapshot(path);

  // Get the NodeId for our test object
  const objectNodeIdRoot = ChromeUtils.getObjectNodeId(testObject);
  const objectNodeIdFoo = ChromeUtils.getObjectNodeId(testObject.foo);
  const objectNodeIdBar = ChromeUtils.getObjectNodeId(testObject.foo.bar);

  // Also try to ensure that this is the right object via its retained path
  const shortestPaths = snapshot.computeShortestPaths(
    objectNodeIdRoot,
    [objectNodeIdBar],
    50
  );
  ok(shortestPaths);
  ok(shortestPaths instanceof Map);
  ok(
    shortestPaths.size == 1,
    "We get only one path between the root object and bar object"
  );

  const paths = shortestPaths.get(objectNodeIdBar);
  ok(paths.length == 1, "There is only one path between root and bar");
  ok(
    paths[0].length == 2,
    "The shortest path is made of two edges: foo and bar"
  );

  const [path1, path2] = paths[0];
  ok(
    path1.predecessor == objectNodeIdRoot,
    "The first edge goes from the root object"
  );
  ok(path1.edge == "foo", "The first edge is the foo attribute");

  ok(
    path2.predecessor == objectNodeIdFoo,
    "The second edge goes from the foo object"
  );
  ok(path2.edge == "bar", "The first edge is the bar attribute");

  do_test_finished();
}
