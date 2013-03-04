load(libdir + "asserts.js");

function testScanThrows() {
  // Throw on empty
  assertThrowsInstanceOf(function () {
    var p = new ParallelArray([]);
    p.scan(function (v, p) { return v*p; });
  }, Error);

  // Throw on not function
  assertThrowsInstanceOf(function () {
    var p = new ParallelArray([1]);
    p.scan(42);
  }, TypeError);
}

// FIXME(bug 844886) sanity check argument types
// testScanThrows();
