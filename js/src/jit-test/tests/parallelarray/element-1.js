function testElement() {
  // Test getting element from 1D
  var a = [1,{},"a",false]
  var p = new ParallelArray(a);
  for (var i = 0; i < a.length; i++) {
    assertEq(p.get(i), p.get(i));
    assertEq(p.get(i), a[i]);
  }
  // Test out of bounds
  assertEq(p.get(42), undefined);
}

testElement();
