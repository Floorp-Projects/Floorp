function testGetBounds() {
  var p = new ParallelArray([1,2,3,4]);
  assertEq(p.get(42), undefined);
}

testGetBounds();
