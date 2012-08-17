function bracket(s) {
  return "<" + s + ">";
}

function testFilterSome() {
  var p = new ParallelArray([0,1,2,3,4]);
  var evenBelowThree = p.map(function (i) { return ((i%2) === 0) && (i < 3); });
  var r = p.filter(evenBelowThree);
  assertEq(r.toString(), bracket([0,2].join(",")));
  var p = new ParallelArray([5,2], function (i,j) { return i; });
  var evenBelowThree = p.map(function (i) { return ((i[0]%2) === 0) && (i[0] < 3); });
  var r = p.filter(evenBelowThree);
  assertEq(r.toString(), bracket(["<0,0>","<2,2>"].join(",")));
}

testFilterSome();
