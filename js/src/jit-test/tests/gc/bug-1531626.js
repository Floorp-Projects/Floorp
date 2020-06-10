// Test that setting the nursery size works as expected.
//
// It's an error to set the minimum size greater than the maximum
// size. Parameter values are rounded to the nearest legal nursery
// size.

load(libdir + "asserts.js");

const chunkSizeKB = gcparam('chunkBytes') / 1024;

var testSizesKB = [128, 129, 255, 256, 516, 1023, 1024, 3*1024, 4*1024+1, 16*1024];

// Valid maximum sizes must be >= 1MB.
var testMaxSizesKB = testSizesKB.filter(x => x >= 1024);

for (var max of testMaxSizesKB) {
  // Don't test minimums greater than the maximum.
  for (var min of testSizesKB.filter(x => x <= max)) {
    setMinMax(min, max);
  }
}

// The above loops raised the nursery size.  Now reduce it to ensure that
// forcibly-reducing it works correctly.
gcparam('minNurseryBytes', 256*1024); // need to avoid min > max;
setMinMax(256, 1024);

// Try an invalid configuration.
assertErrorMessage(
  () => setMinMax(2*1024, 1024),
  Object,
  "Parameter value out of range");

function setMinMax(min, max) {
  // Set the maximum first so that we don't hit a case where max < min.
  gcparam('maxNurseryBytes', max * 1024);
  gcparam('minNurseryBytes', min * 1024);
  assertEq(gcparam('maxNurseryBytes'), nearestLegalSize(max) * 1024);
  assertEq(gcparam('minNurseryBytes'), nearestLegalSize(min) * 1024);
  allocateSomeThings();
  gc();
}

function allocateSomeThings() {
  for (var i = 0; i < 1000; i++) {
    var obj = { an: 'object', with: 'fields' };
  }
}

function nearestLegalSize(sizeKB) {
  let step = sizeKB >= chunkSizeKB ? chunkSizeKB : 4;
  return round(sizeKB, step);
}

function round(x, y) {
  x += y / 2;
  return x - (x % y);
}
