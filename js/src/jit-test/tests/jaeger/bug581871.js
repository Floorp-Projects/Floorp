function test(p) {
  var alwaysFalse = p && !p;
  var k = [];
  var g;
  if (!alwaysFalse) {
    k[0] = g = alwaysFalse ? "failure" : "success";
    return g;
  }
}
assertEq(test("anything"), "success");
