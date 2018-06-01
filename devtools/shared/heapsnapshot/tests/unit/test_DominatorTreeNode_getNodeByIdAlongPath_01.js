/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that we can find the node with the given id along the specified path.

const node3000 = makeTestDominatorTreeNode({ nodeId: 3000 });

const node2000 = makeTestDominatorTreeNode({ nodeId: 2000 }, [
  makeTestDominatorTreeNode({}),
  node3000,
  makeTestDominatorTreeNode({}),
]);

const node1000 = makeTestDominatorTreeNode({ nodeId: 1000 }, [
  makeTestDominatorTreeNode({}),
  node2000,
  makeTestDominatorTreeNode({}),
]);

const tree = node1000;

const path = [1000, 2000, 3000];

const tests = [
  { id: 1000, expected: node1000 },
  { id: 2000, expected: node2000 },
  { id: 3000, expected: node3000 },
];

function run_test() {
  for (const { id, expected } of tests) {
    const actual = DominatorTreeNode.getNodeByIdAlongPath(id, tree, path);
    equal(actual, expected, `We should have got the node with id = ${id}`);
  }

  equal(null,
        DominatorTreeNode.getNodeByIdAlongPath(999999999999, tree, path),
        "null is returned for nodes that are not even in the tree");

  const lastNodeId = tree.children[tree.children.length - 1].nodeId;
  equal(null,
        DominatorTreeNode.getNodeByIdAlongPath(lastNodeId, tree, path),
        "null is returned for nodes that are not along the path");
}
