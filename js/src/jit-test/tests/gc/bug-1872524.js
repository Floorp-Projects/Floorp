// Attempt to break invariant that smallHeapIncrementalLimit >=
// largeHeapIncrementalLimit and check that it is maintained.

function checkInvariant() {
  return gcparam("smallHeapIncrementalLimit") >=
         gcparam("largeHeapIncrementalLimit");
}

assertEq(checkInvariant(), true);

const smallLimit = gcparam("smallHeapIncrementalLimit");
gcparam("largeHeapIncrementalLimit", smallLimit + 1);
assertEq(checkInvariant(), true);

const largeLimit = gcparam("largeHeapIncrementalLimit");
gcparam("smallHeapIncrementalLimit", largeLimit - 1);
assertEq(checkInvariant(), true);
