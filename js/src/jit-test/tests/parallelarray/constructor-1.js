load(libdir + "eqArrayHelper.js");

function buildSimple() {
  // Simple constructor
  var a = [1,2,3,4,5];
  var p = new ParallelArray(a);
  assertEqArray(p, a);
  var a2 = a.slice();
  a[0] = 9;
  // No sharing
  assertEqArray(p, a2);
}

buildSimple();
