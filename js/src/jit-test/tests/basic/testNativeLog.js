function testNativeLog() {
  var a = new Array(5);
  for (var i = 0; i < 5; i++) {
    a[i] = Math.log(Math.pow(Math.E, 10));
  }
  return a.join(",");
}
assertEq(testNativeLog(), "10,10,10,10,10");
