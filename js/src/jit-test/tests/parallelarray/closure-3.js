load(libdir + "parallelarray-helpers.js");

function testClosureCreationAndInvocation() {
  var a = range(1, 65);
  var p = new ParallelArray(a);
  function etaadd1(v) { return (function (x) { return x+1; })(v); };
  // eta-expansion is (or at least can be) treated as call with unknown target
  var m = p.map(etaadd1, {mode: "par", expect: "success"});
  assertEq(m.get(1), 3); // (\x.x+1) 2 == 3
}

testClosureCreationAndInvocation();
