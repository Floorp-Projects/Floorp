function test() {
  function mk(i, j, k, l) { return i*1000000+j*10000+k*100+l; }

  var N = 2;
  var M = 4;
  var O = 6;
  var P = 8;

  print("Computing");
  var p = new ParallelArray([N,M,O,P], mk);

  print("Checking");
  for (var i = 0; i < N; i++) {
    for (var j = 0; j < M; j++) {
      for (var k = 0; k < O; k++) {
        for (var l = 0; l < P; l++) {
          assertEq(p.get(i, j, k, l), mk(i, j, k, l));
          assertEq(p.get(i, j).get(k, l), mk(i, j, k, l));
          assertEq(p.get(i).get(j).get(k).get(l), mk(i, j, k, l));
        }
      }
    }
  }
}

test();
