function test() {
  function mk(i, j, k) { return i*10000+j*100+k; }

  var N = 10;
  var M = 20;
  var O = 30;

  print("Computing");
  var p = new ParallelArray([N,M,O], mk);

  print("Checking");
  for (var i = 0; i < N; i++) {
    for (var j = 0; j < M; j++) {
      for (var k = 0; k < O; k++) {
        assertEq(p.get(i, j, k), mk(i, j, k));
        assertEq(p.get(i, j).get(k), mk(i, j, k));
        assertEq(p.get(i).get(j).get(k), mk(i, j, k));
        assertEq(p.get(i).get(j, k), mk(i, j, k));
      }
    }
  }
}

test();
