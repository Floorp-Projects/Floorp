function bracket(s) {
  return "<" + s + ">";
}

function testMap() {
    var p = new ParallelArray([0,1,2,3,4]);
    var m = p.map(function (v) { return v+1; });
    assertEq(m.toString(), bracket([1,2,3,4,5].join(",")));
}

testMap();
