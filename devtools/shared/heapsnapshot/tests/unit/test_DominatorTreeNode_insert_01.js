/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can insert new children into an existing DominatorTreeNode tree.

const tree = makeTestDominatorTreeNode({ nodeId: 1000 }, [
  makeTestDominatorTreeNode({}),
  makeTestDominatorTreeNode({ nodeId: 2000 }, [
    makeTestDominatorTreeNode({}),
    makeTestDominatorTreeNode({ nodeId: 3000 }),
    makeTestDominatorTreeNode({}),
  ]),
  makeTestDominatorTreeNode({}),
]);

const path = [1000, 2000, 3000];

const newChildren = [
  makeTestDominatorTreeNode({ parentId: 3000 }),
  makeTestDominatorTreeNode({ parentId: 3000 }),
];

const moreChildrenAvailable = false;

const expected = {
  nodeId: 1000,
  parentId: undefined,
  label: undefined,
  shallowSize: 1,
  retainedSize: 7,
  children: [
    {
      nodeId: 0,
      label: undefined,
      shallowSize: 1,
      retainedSize: 1,
      parentId: 1000,
      moreChildrenAvailable: true,
      children: undefined,
    },
    {
      nodeId: 2000,
      label: undefined,
      shallowSize: 1,
      retainedSize: 4,
      parentId: 1000,
      children: [
        {
          nodeId: 1,
          label: undefined,
          shallowSize: 1,
          retainedSize: 1,
          parentId: 2000,
          moreChildrenAvailable: true,
          children: undefined,
        },
        {
          nodeId: 3000,
          label: undefined,
          shallowSize: 1,
          retainedSize: 1,
          parentId: 2000,
          children: [
            {
              nodeId: 7,
              parentId: 3000,
              label: undefined,
              shallowSize: 1,
              retainedSize: 1,
              moreChildrenAvailable: true,
              children: undefined,
            },
            {
              nodeId: 8,
              parentId: 3000,
              label: undefined,
              shallowSize: 1,
              retainedSize: 1,
              moreChildrenAvailable: true,
              children: undefined,
            },
          ],
          moreChildrenAvailable: false
        },
        {
          nodeId: 3,
          label: undefined,
          shallowSize: 1,
          retainedSize: 1,
          parentId: 2000,
          moreChildrenAvailable: true,
          children: undefined,
        },
      ],
      moreChildrenAvailable: true
    },
    {
      nodeId: 5,
      label: undefined,
      shallowSize: 1,
      retainedSize: 1,
      parentId: 1000,
      moreChildrenAvailable: true,
      children: undefined,
    }
  ],
  moreChildrenAvailable: true
};

function run_test() {
  assertDominatorTreeNodeInsertion(tree, path, newChildren, moreChildrenAvailable, expected);
}
