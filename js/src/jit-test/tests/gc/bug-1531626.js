// Test that setting the nursery size works as expected.

load(libdir + "asserts.js");

var testSizesKB = [128, 129, 255, 256, 1023, 1024, 3*1024, 4*1024+1, 16*1024];

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

// try an invalid configuration.
assertErrorMessage(
  () => setMinMax(2*1024, 1024),
  Object,
  "Parameter value out of range");

function setMinMax(min, max) {
  // Set the maximum first so that we don't hit a case where max < min.
  gcparam('maxNurseryBytes', max * 1024);
  gcparam('minNurseryBytes', min * 1024);
  assertEq(max * 1024, gcparam('maxNurseryBytes'));
  assertEq(min * 1024, gcparam('minNurseryBytes'));
  allocateSomeThings();
  gc();
}

function allocateSomeThings() {
  for (var i = 0; i < 1000; i++) {
    var obj = { an: 'object', with: 'fields' };
  }
}

