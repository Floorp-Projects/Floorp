load(libdir + "asserts.js");

function testArrayThrows() {
  // Test kernel not being callable.
  var a = [1,2,3,4,5,6,7,8];
  for (var [opName, args] in ["mapPar", [{}],
                              "filterPar", [{}],
                              "reducePar", [{}],
                              "scanPar", [{}]])
  {
    assertThrowsInstanceOf(function () { a[opName].apply(a, args); }, TypeError);
  }

  assertThrowsInstanceOf(function () {
    a.scatterPar(a, 0, {});
  }, TypeError);

  assertThrowsInstanceOf(function () {
    Array.buildPar(1024, {});
  }, TypeError);

  // Test bad lengths.
  assertThrowsInstanceOf(function () {
    Array.buildPar(0xffffffff + 1, function () {});
  }, RangeError);

  assertThrowsInstanceOf(function () {
    a.scatterPar(a, 0, function () {}, 0xffffffff + 1);
  }, RangeError);

  // Test empty reduction.
  for (var opName in ["reducePar", "scanPar"]) {
    assertThrowsInstanceOf(function () {
      var a = [];
      a[opName](function (v, p) { return v * p; });
    }, TypeError);
  }

  // Test scatter:
  //  - no conflict function.
  //  - out of bounds
  var p = [1,2,3,4,5];

  assertThrowsInstanceOf(function () {
    p.scatterPar([0,1,0,3,4]);
  }, Error);

  assertThrowsInstanceOf(function () {
    p.scatterPar([0,1,0,3,11]);
  }, Error);
}

if (getBuildConfiguration().parallelJS)
  testArrayThrows();
