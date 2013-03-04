function testHoles() {
  function f1(a) { return a * 42; }
  function f2(a,b) { return a * b; }
  // Don't crash when getting holes out.
  //
  // We do the multiplications below instead of asserting against undefined to
  // force the VM to call a conversion function, which will crash if the value
  // we got out is a JS_ARRAY_HOLE.
  var p = new ParallelArray([,1]);
  assertEq(p.get(0) * 42, NaN);
  var m = p.map(f1);
  assertEq(m.get(0), NaN);
  assertEq(m.get(1), 42);
  var r = p.reduce(f2);
  assertEq(r, NaN);
  var s = p.scan(f2);
  assertEq(s.get(0) * 42, NaN);
  assertEq(s.get(1), NaN);
  var k = p.scatter([1,0]);
  assertEq(k.get(0), 1);
  assertEq(k[1] * 42, NaN);
  var l = p.filter(function (e, i) { return i == 0; });
  assertEq(l.get(0) * 42, NaN);
  var p2 = p.partition(1);
  assertEq(p2.get(0).get(0) * 42, NaN);
  var g = p.get(0);
  assertEq(g * 42, NaN);
}

testHoles();
