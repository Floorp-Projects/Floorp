function testClosureCreation() {
  var a = [ 1, 2, 3, 4, 5, 6, 7, 8, 9,10,
            11,12,13,14,15,16,17,18,19,20,
            21,22,23,24,25,26,27,28,29,30,
            31,32,33,34,35,36,27,38,39,40];
  var p = new ParallelArray(a);
  var makeadd1 = function (v) { return function (x) { return x+1; }; };
  var m = p.map(makeadd1, {mode: "par", expect: "success"});
  assertEq(m.get(1)(2), 3); // (\x.x+1) 2 == 3
}

if (getBuildConfiguration().parallelJS)
  testClosureCreation();
