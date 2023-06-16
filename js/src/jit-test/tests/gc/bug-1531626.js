// Test that setting the nursery size works as expected.
//
// It's an error to set the minimum size greater than the maximum
// size. Parameter values are rounded to the nearest legal nursery
// size.

load(libdir + "asserts.js");

const chunkSizeKB = gcparam('chunkBytes') / 1024;
const pageSizeKB = gcparam('systemPageSizeKB');

const testSizesKB = [128, 129, 255, 256, 516, 1023, 1024, 3*1024, 4*1024+1, 16*1024];

// Valid maximum sizes must be >= 1MB.
const testMaxSizesKB = testSizesKB.filter(x => x >= 1024);

for (let max of testMaxSizesKB) {
  // Don't test minimums greater than the maximum.
  for (let min of testSizesKB.filter(x => x <= max)) {
    setMinMax(min, max);
  }
}

// The above loops raised the nursery size.  Now reduce it to ensure that
// forcibly-reducing it works correctly.
setMinMax(256, 1024);

// Try invalid configurations.
const badSizesKB = [ 0, 1, 129 * 1024];
function assertParamOutOfRange(f) {
  assertErrorMessage(f, Object, "Parameter value out of range");
}
for (let size of badSizesKB) {
  assertParamOutOfRange(() => gcparam('minNurseryBytes', size * 1024));
  assertParamOutOfRange(() => gcparam('maxNurseryBytes', size * 1024));
}

function setMinMax(min, max) {
  gcparam('minNurseryBytes', min * 1024);
  gcparam('maxNurseryBytes', max * 1024);
  assertEq(gcparam('minNurseryBytes'), nearestLegalSize(min) * 1024);
  assertEq(gcparam('maxNurseryBytes'), nearestLegalSize(max) * 1024);
  allocateSomeThings();
  gc();
}

function allocateSomeThings() {
  for (let i = 0; i < 1000; i++) {
    let obj = { an: 'object', with: 'fields' };
  }
}

function nearestLegalSize(sizeKB) {
  let step = sizeKB >= chunkSizeKB ? chunkSizeKB : pageSizeKB;
  return round(sizeKB, step);
}

function round(x, y) {
  x += y / 2;
  return x - (x % y);
}
