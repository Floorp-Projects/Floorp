/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test inserting new children into an existing DominatorTreeNode at the root.

const tree = makeTestDominatorTreeNode({ nodeId: 666 }, [
  makeTestDominatorTreeNode({}),
  makeTestDominatorTreeNode({}, [
    makeTestDominatorTreeNode({}),
    makeTestDominatorTreeNode({}),
    makeTestDominatorTreeNode({}),
  ]),
  makeTestDominatorTreeNode({}),
]);

const path = [666];

const newChildren = [
  makeTestDominatorTreeNode({
    nodeId: 777,
    parentId: 666
  }),
  makeTestDominatorTreeNode({
    nodeId: 888,
    parentId: 666
  }),
];

const moreChildrenAvailable = false;

const expected = {
  nodeId: 666,
  label: undefined,
  parentId: undefined,
  shallowSize: 1,
  retainedSize: 7,
  children: [
    {
      nodeId: 0,
      label: undefined,
      shallowSize: 1,
      retainedSize: 1,
      parentId: 666,
      moreChildrenAvailable: true,
      children: undefined
    },
    {
      nodeId: 4,
      label: undefined,
      shallowSize: 1,
      retainedSize: 4,
      parentId: 666,
      children: [
        {
          nodeId: 1,
          label: undefined,
          shallowSize: 1,
          retainedSize: 1,
          parentId: 4,
          moreChildrenAvailable: true,
          children: undefined
        },
        {
          nodeId: 2,
          label: undefined,
          shallowSize: 1,
          retainedSize: 1,
          parentId: 4,
          moreChildrenAvailable: true,
          children: undefined
        },
        {
          nodeId: 3,
          label: undefined,
          shallowSize: 1,
          retainedSize: 1,
          parentId: 4,
          moreChildrenAvailable: true,
          children: undefined
        }
      ],
      moreChildrenAvailable: true
    },
    {
      nodeId: 5,
      label: undefined,
      shallowSize: 1,
      retainedSize: 1,
      parentId: 666,
      moreChildrenAvailable: true,
      children: undefined
    },
    {
      nodeId: 777,
      label: undefined,
      shallowSize: 1,
      retainedSize: 1,
      parentId: 666,
      moreChildrenAvailable: true,
      children: undefined
    },
    {
      nodeId: 888,
      label: undefined,
      shallowSize: 1,
      retainedSize: 1,
      parentId: 666,
      moreChildrenAvailable: true,
      children: undefined
    }
  ],
  moreChildrenAvailable: false
};

function run_test() {
  assertDominatorTreeNodeInsertion(tree, path, newChildren, moreChildrenAvailable, expected);
}
