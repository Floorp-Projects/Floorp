function testGet() {
  var a = [1,2,3,4,5];
  var p = new ParallelArray(a);
  for (var i = 0; i < a.length; i++)
    assertEq(p.get(i), a[i]);
}

testGet();
