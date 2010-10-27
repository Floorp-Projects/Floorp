function testArrayDensityChange() {
  var x = [];
  var count = 0;
  for (var i=0; i < 100; ++i) {
    x[i] = "asdf";
  }
  for (var i=0; i < x.length; ++i) {
      if (i == 51)
      {
        x[199] = "asdf";
      }
      if (x[i])
        count += x[i].length;
  }
  return count;
}
assertEq(testArrayDensityChange(), 404);
