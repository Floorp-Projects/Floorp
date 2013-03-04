function test() {
  function mk(i, j) { return i*100+j; }

  var N = 22;
  var M = 44;

  var p = new ParallelArray([N,M], mk);
  for (var i = 0; i < N; i++) {
    for (var j = 0; j < M; j++) {
      print([i, j]);
      assertEq(p.get(i, j), mk(i, j));
      assertEq(p.get(i).get(j), mk(i, j));
    }
  }
}

test();
