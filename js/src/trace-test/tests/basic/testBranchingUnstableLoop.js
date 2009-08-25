function testBranchingUnstableLoop() {
  var x = 0;
  for (var i=0; i < 100; ++i) {
    if (i == 51) {
      x += 10.1;
    }
    x++;
  }
  return x;
}
assertEq(testBranchingUnstableLoop(), 110.1);
