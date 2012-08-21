load(libdir + "asserts.js");

function testGetThrows() {
  // Throw if argument not object
  var p = new ParallelArray([1,2,3,4]);
  assertThrowsInstanceOf(function () { p.get(42); }, TypeError);
  assertThrowsInstanceOf(function () { p.get(.42); }, TypeError);
  var p = new ParallelArray([2,2], function (i,j) { return i+j; });
  assertThrowsInstanceOf(function () {
    p.get({ 0: 1, 1: 0, testGet: 2 });
  }, TypeError);
}

testGetThrows();
