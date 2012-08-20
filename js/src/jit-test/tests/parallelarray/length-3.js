function testLength() {
  // Test length attributes
  var p = new ParallelArray([1,2,3,4]);
  var desc = Object.getOwnPropertyDescriptor(p, "length");
  assertEq(desc.enumerable, false);
  assertEq(desc.configurable, false);
  assertEq(desc.writable, false);
  assertEq(desc.value, 4);
}

testLength();
