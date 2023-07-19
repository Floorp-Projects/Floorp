// Check that we switch to allocating in the tenure heap after the first
// nursery collection.

function buildObjectTree(depth) {
  if (depth === 0) {
    return "leaf";
  }

  return {
    left: buildObjectTree(depth - 1),
    right: buildObjectTree(depth - 1)
  };
}

function buildArrayTree(depth) {
  if (depth === 0) {
    return [];
  }

  return [
    buildArrayTree(depth - 1),
    buildArrayTree(depth - 1)
  ];
}

function testRoundTrip(depth, objectTree, expectedNurseryAllocated) {
  const input = objectTree ? buildObjectTree(depth) : buildArrayTree(depth);

  gc();
  const initialMinorNumber = gcparam('minorGCNumber');

  const output = deserialize(serialize(input));
  checkHeap(output, depth, objectTree, expectedNurseryAllocated);

  const minorCollections = gcparam('minorGCNumber') - initialMinorNumber;
  const expectedMinorCollections = expectedNurseryAllocated ? 0 : 1;
  assertEq(minorCollections, expectedMinorCollections);
}

function checkHeap(tree, depth, objectTree, expectedNurseryAllocated) {
  const counts = countHeapLocations(tree, objectTree);

  const total = counts.nursery + counts.tenured;
  assertEq(total, (2 ** (depth + 1)) - 1);

  if (expectedNurseryAllocated) {
    assertEq(counts.tenured, 0);
    assertEq(counts.nursery >= 1, true);
  } else {
    assertEq(counts.tenured >= 1, true);
    // We get a single nursery allocation when we trigger minor GC.
    assertEq(counts.nursery <= 1, true);
  }
}

function countHeapLocations(tree, objectTree, counts) {
  if (!counts) {
    counts = {nursery: 0, tenured: 0};
  }

  if (isNurseryAllocated(tree)) {
    counts.nursery++;
  } else {
    counts.tenured++;
  }

  if (objectTree) {
    if (tree !== "leaf") {
      countHeapLocations(tree.left, objectTree, counts);
      countHeapLocations(tree.right, objectTree, counts);
    }
  } else {
    if (tree.length !== 0) {
      countHeapLocations(tree[0], objectTree, counts);
      countHeapLocations(tree[1], objectTree, counts);
    }
  }

  return counts;
}

gczeal(0);
gcparam('minNurseryBytes', 1024 * 1024);
gcparam('maxNurseryBytes', 1024 * 1024);
gc();

testRoundTrip(1, true, true);
testRoundTrip(1, false, true);
testRoundTrip(4, true, true);
testRoundTrip(4, false, true);
testRoundTrip(15, true, false);
testRoundTrip(15, false, false);
