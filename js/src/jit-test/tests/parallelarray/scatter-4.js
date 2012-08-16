function bracket(s) {
  return "<" + s + ">";
}

function testScatterDefault() {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([0,2,4], 9);
  assertEq(r.toString(), bracket([1,9,2,9,3].join(",")));
}

testScatterDefault();

