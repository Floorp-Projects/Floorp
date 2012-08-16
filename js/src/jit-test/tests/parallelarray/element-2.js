function testElement() {
  // Test getting element from higher dimension
  var p = new ParallelArray([2,2,2], function () { return 0; });
  assertEq(p[0].toString(), "<<0,0>,<0,0>>");
  // Should create new wrapper
  assertEq(p[0] !== p[0], true);
  // Test out of bounds
  assertEq(p[42], undefined);
}

testElement();
