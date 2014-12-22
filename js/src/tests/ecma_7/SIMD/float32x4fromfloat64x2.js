// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;

var summary = 'float32x4 fromFloat64x2';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float64x2(1, 2);
  var c = float32x4.fromFloat64x2(a);
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 0);
  assertEq(c.w, 0);

  var d = float64x2(-0, NaN);
  var f = float32x4.fromFloat64x2(d);
  assertEq(f.x, -0);
  assertEq(f.y, NaN);
  assertEq(f.z, 0);
  assertEq(f.w, 0);

  var g = float64x2(Infinity, -Infinity);
  var i = float32x4.fromFloat64x2(g);
  assertEq(i.x, Infinity);
  assertEq(i.y, -Infinity);
  assertEq(i.z, 0);
  assertEq(i.w, 0);

  var j = Math.pow(2, 25) - 1;
  var k = -Math.pow(2, 25);
  var l = float64x2(j, k);
  var m = float32x4.fromFloat64x2(l);
  assertEq(m.x, Math.fround(j));
  assertEq(m.y, Math.fround(k));
  assertEq(m.z, 0);
  assertEq(m.w, 0);

  var o = Math.pow(2, 1000);
  var p = Math.pow(2, -1000);
  var q = float64x2(o, p);
  var r = float32x4.fromFloat64x2(q);
  assertEq(r.x, Math.fround(o));
  assertEq(r.y, Math.fround(p));
  assertEq(r.z, 0);
  assertEq(r.w, 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

