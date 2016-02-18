/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we correctly set `moreChildrenAvailable` when doing a partial
// traversal of a dominator tree to create the initial incrementally loaded
// `DominatorTreeNode` tree.

// `tree` maps parent to children:
//
// 100
//   |- 200
//   |    |- 500
//   |    |- 600
//   |    `- 700
//   |- 300
//   |    |- 800
//   |    |- 900
//   `- 400
//        |- 1000
//        |- 1100
//        `- 1200
const tree = new Map([
  [100, [200, 300, 400]],
  [200, [500, 600, 700]],
  [300, [800, 900]],
  [400, [1000, 1100, 1200]]
]);

const mockDominatorTree = {
  root: 100,
  getRetainedSize: _ => 10,
  getImmediatelyDominated: id => (tree.get(id) || []).slice()
};

const mockSnapshot = {
  describeNode: _ => ({
    objects: { count: 0, bytes: 0 },
    strings: { count: 0, bytes: 0 },
    scripts: { count: 0, bytes: 0 },
    other: { SomeType: { count: 1, bytes: 10 } }
  })
};

const breakdown = {
  by: "coarseType",
  objects: { by: "count", count: true, bytes: true },
  strings: { by: "count", count: true, bytes: true },
  scripts: { by: "count", count: true, bytes: true },
  other: {
    by: "internalType",
    then: { by: "count", count: true, bytes: true }
  },
};

const expected = {
  nodeId: 100,
  label: [
    "other",
    "SomeType"
  ],
  shallowSize: 10,
  retainedSize: 10,
  shortestPaths: undefined,
  children: [
    {
      nodeId: 200,
      label: [
        "other",
        "SomeType"
      ],
      shallowSize: 10,
      retainedSize: 10,
      parentId: 100,
      shortestPaths: undefined,
      children: [
        {
          nodeId: 500,
          label: [
            "other",
            "SomeType"
          ],
          shallowSize: 10,
          retainedSize: 10,
          parentId: 200,
          moreChildrenAvailable: false,
          shortestPaths: undefined,
          children: undefined
        },
        {
          nodeId: 600,
          label: [
            "other",
            "SomeType"
          ],
          shallowSize: 10,
          retainedSize: 10,
          parentId: 200,
          moreChildrenAvailable: false,
          shortestPaths: undefined,
          children: undefined
        }
      ],
      moreChildrenAvailable: true
    },
    {
      nodeId: 300,
      label: [
        "other",
        "SomeType"
      ],
      shallowSize: 10,
      retainedSize: 10,
      parentId: 100,
      shortestPaths: undefined,
      children: [
        {
          nodeId: 800,
          label: [
            "other",
            "SomeType"
          ],
          shallowSize: 10,
          retainedSize: 10,
          parentId: 300,
          moreChildrenAvailable: false,
          shortestPaths: undefined,
          children: undefined
        },
        {
          nodeId: 900,
          label: [
            "other",
            "SomeType"
          ],
          shallowSize: 10,
          retainedSize: 10,
          parentId: 300,
          moreChildrenAvailable: false,
          shortestPaths: undefined,
          children: undefined
        }
      ],
      moreChildrenAvailable: false
    }
  ],
  moreChildrenAvailable: true,
  parentId: undefined,
};

function run_test() {
  // Traverse the whole depth of the test tree, but one short of the number of
  // siblings. This will exercise the moreChildrenAvailable handling for
  // siblings.
  const actual = DominatorTreeNode.partialTraversal(mockDominatorTree,
                                                    mockSnapshot,
                                                    breakdown,
                                                    /* maxDepth = */ 4,
                                                    /* siblings = */ 2);

  dumpn("Expected = " + JSON.stringify(expected, null, 2));
  dumpn("Actual = " + JSON.stringify(actual, null, 2));

  assertStructurallyEquivalent(expected, actual);
}
