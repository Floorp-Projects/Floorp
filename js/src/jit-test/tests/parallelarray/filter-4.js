function bracket(s) {
  return "<" + s + ">";
}

function testFilterMisc() {
  var p = new ParallelArray([0,1,2]);
  // Test array
  var r = p.filter([true, false, true]);
  assertEq(r.toString(), bracket([0,2].join(",")));
  // Test array-like
  var r = p.filter({ 0: true, 1: false, 2: true, length: 3 });
  assertEq(r.toString(), bracket([0,2].join(",")));
  // Test truthy
  var r = p.filter([1, "", {}]);
  assertEq(r.toString(), bracket([0,2].join(",")));
}

testFilterMisc();
