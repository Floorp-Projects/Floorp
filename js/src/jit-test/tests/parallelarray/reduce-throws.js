load(libdir + "asserts.js");

function testReduceThrows() {
  // Throw on empty
  assertThrowsInstanceOf(function () {
    var p = new ParallelArray([]);
    p.reduce(function (v, p) { return v*p; });
  }, Error);
  // Throw on not function
  assertThrowsInstanceOf(function () {
    var p = new ParallelArray([1]);
    p.reduce(42);
  }, TypeError);
}

// FIXME(bug 844886) sanity check argument types
// testReduceThrows();
