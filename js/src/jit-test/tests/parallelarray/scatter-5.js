function bracket(s) {
  return "<" + s + ">";
}

function testScatterConflict() {
    var p = new ParallelArray([1,2,3,4,5]);
    var r = p.scatter([0,1,0,3,4], 9, function (a,b) { return a+b; });
    assertEq(r.toString(), bracket([4,2,9,4,5].join(",")));
}

testScatterConflict();
