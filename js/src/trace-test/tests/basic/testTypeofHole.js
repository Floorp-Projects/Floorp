function testTypeofHole() {
  var a = new Array(6);
  a[5] = 3;
  for (var i = 0; i < 6; ++i)
    a[i] = typeof a[i];
  return a.join(",");
}
assertEq(testTypeofHole(), "undefined,undefined,undefined,undefined,undefined,number");
