/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test attempting to insert new children into an existing DominatorTreeNode
// tree with a bad path.

const tree = makeTestDominatorTreeNode({}, [
  makeTestDominatorTreeNode({}),
  makeTestDominatorTreeNode({}, [
    makeTestDominatorTreeNode({}),
    makeTestDominatorTreeNode({}),
    makeTestDominatorTreeNode({}),
  ]),
  makeTestDominatorTreeNode({}),
]);

const path = [111111, 222222, 333333];

const newChildren = [
  makeTestDominatorTreeNode({ parentId: 333333 }),
  makeTestDominatorTreeNode({ parentId: 333333 }),
];

const moreChildrenAvailable = false;

const expected = tree;

function run_test() {
  assertDominatorTreeNodeInsertion(tree, path, newChildren, moreChildrenAvailable, expected);
}
