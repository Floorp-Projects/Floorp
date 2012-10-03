load(libdir + "asserts.js");

function testFilterThrows() {
  var p = new ParallelArray([1,2,3,4,5]);

  assertThrowsInstanceOf(function () {
    p.filter({ length: 0xffffffff + 1 });
  }, RangeError);
}

testFilterThrows();
