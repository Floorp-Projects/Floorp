let capture = [];

for (let i = 0; i <= 200; ++i) {
  if (i === 100) {
    enableTrackAllocations();
  }

  // Create a new object through `new Object` and capture the result.
  capture[i & 1] = new Object();

  // Ensure the allocation is properly tracked when inlining `new Object` in CacheIR.
  let data = getAllocationMetadata(capture[i & 1]);
  assertEq(data !== null, i >= 100);
}
