load(libdir + "asserts.js");

function testOverflow() {
  assertThrowsInstanceOf(function () {
    new ParallelArray([0xffffffff + 1], function() { return 0; });
  }, RangeError);
  assertThrowsInstanceOf(function () {
    new ParallelArray({ length: 0xffffffff + 1 });
  }, RangeError);
}

if (getBuildConfiguration().parallelJS) testOverflow();
