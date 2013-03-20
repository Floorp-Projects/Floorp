load(libdir + "asserts.js");

function testThrows() {
  assertThrowsInstanceOf(function () {
    new ParallelArray({ length: 0xffffffff + 1 });
  }, RangeError);
}

if (getBuildConfiguration().parallelJS)
  testThrows();
