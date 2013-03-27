function bracket(s) {
  return "<" + s + ">";
}

function buildArrayLike() {
  // Construct copying from array-like
  var a = { 0: 1, 1: 2, 2: 3, 3: 4, length: 4 };
  var p = new ParallelArray(a);
  var e = Array.prototype.join.call(a, ",");
  assertEq(p.toString(), e);
}

if (getBuildConfiguration().parallelJS)
  buildArrayLike();
