function testBranchingUnstableObject() {
  var x = {s: "a"};
  var t = "";
  for (var i=0; i < 100; ++i) {
      if (i == 51)
      {
        x.s = 5;
      }
      t += x.s;
  }
  return t.length;
}
assertEq(testBranchingUnstableObject(), 100);
