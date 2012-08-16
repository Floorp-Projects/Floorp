function bracket(s) {
  return "<" + s + ">";
}

function buildSimple() {
  // Simple constructor
  var a = [1,2,3,4,5];
  var p = new ParallelArray(a);
  var e = a.join(",");
  assertEq(p.toString(), bracket(e));
  a[0] = 9;
  // No sharing
  assertEq(p.toString(), bracket(e));
}

buildSimple();
