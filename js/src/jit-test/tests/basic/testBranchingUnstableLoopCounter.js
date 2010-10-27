function testBranchingUnstableLoopCounter() {
  var x = 0;
  for (var i=0; i < 100; ++i) {
    if (i == 51) {
      i += 1.1;
    }
    x++;
  }
  return x;
}
assertEq(testBranchingUnstableLoopCounter(), 99);
