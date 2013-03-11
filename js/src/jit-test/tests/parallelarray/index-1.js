function test() {
  var N = 22;
  var p = new ParallelArray([N], function(i) { return i; });
  for (var i = 0; i < N; i++)
    assertEq(p.get(i), i);
}

test();
