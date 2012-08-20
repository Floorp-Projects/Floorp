load(libdir + "asserts.js");

function testScatterThrows() {
  // Throw on conflict with no resolution function
  assertThrowsInstanceOf(function () {
    var p = new ParallelArray([1,2,3,4,5]);
    var r = p.scatter([0,1,0,3,4]);
  }, Error);
  // Throw on out of bounds
  assertThrowsInstanceOf(function () {
    var p = new ParallelArray([1,2,3,4,5]);
    var r = p.scatter([0,1,0,3,11]);
  }, Error);
}

testScatterThrows();
