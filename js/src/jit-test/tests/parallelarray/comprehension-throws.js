load(libdir + "asserts.js");

function buildComprehension() {
  // Throws if elemental fun not callable
  assertThrowsInstanceOf(function () {
    var p = new ParallelArray([2,2], undefined);
  }, TypeError);
  assertThrowsInstanceOf(function () {
    var p = new ParallelArray(2, /x/);
  }, TypeError);
  assertThrowsInstanceOf(function () {
    var p = new ParallelArray(/x/, /x/);
  }, TypeError);
  assertThrowsInstanceOf(function () {
    new ParallelArray([0xffffffff + 1], function() { return 0; });
  }, RangeError);
  assertThrowsInstanceOf(function () {
    new ParallelArray(0xffffffff + 1, function() { return 0; });
  }, RangeError);
  assertThrowsInstanceOf(function () {
    new ParallelArray([0xfffff, 0xffff], function() { return 0; });
  }, RangeError);
}

// FIXME(bug 844887) throw correct exception
// if (getBuildConfiguration().parallelJS)
//   buildComprehension();
