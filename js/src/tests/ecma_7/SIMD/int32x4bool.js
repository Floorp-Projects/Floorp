// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1106428;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 bool';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function tryEmulateUndefined() {
    if (typeof objectEmulatingUndefined !== 'undefined')
        return objectEmulatingUndefined();
    return undefined;
}

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = int32x4.bool(true, false, true, false);
  assertEq(a.x, -1);
  assertEq(a.y, 0);
  assertEq(a.z, -1);
  assertEq(a.w, 0);

  var b = int32x4.bool(5, 0, 1, -2);
  assertEq(b.x, -1);
  assertEq(b.y, 0);
  assertEq(b.z, -1);
  assertEq(b.w, -1);

  var c = int32x4.bool(1.23, 13.37, 42.99999999, 0.000001);
  assertEq(c.x, -1);
  assertEq(c.y, -1);
  assertEq(c.z, -1);
  assertEq(c.w, -1);

  var d = int32x4.bool("string", "", "1", "0");
  assertEq(d.x, -1);
  assertEq(d.y, 0);
  assertEq(d.z, -1);
  assertEq(d.w, -1);

  var e = int32x4.bool([], [1, 2, 3], SIMD.int32x4(1, 2, 3, 4), function() { return 0; });
  assertEq(e.x, -1);
  assertEq(e.y, -1);
  assertEq(e.z, -1);
  assertEq(e.w, -1);

  var f = int32x4.bool(undefined, null, {}, tryEmulateUndefined());
  assertEq(f.x, 0);
  assertEq(f.y, 0);
  assertEq(f.z, -1);
  assertEq(f.w, 0);

  var g = int32x4.bool();
  assertEq(g.x, 0);
  assertEq(g.y, 0);
  assertEq(g.z, 0);
  assertEq(g.w, 0);

  var h = int32x4.bool(true);
  assertEq(h.x, -1);
  assertEq(h.y, 0);
  assertEq(h.z, 0);
  assertEq(h.w, 0);

  var i = int32x4.bool(true, true);
  assertEq(i.x, -1);
  assertEq(i.y, -1);
  assertEq(i.z, 0);
  assertEq(i.w, 0);

  var j = int32x4.bool(true, true, true);
  assertEq(j.x, -1);
  assertEq(j.y, -1);
  assertEq(j.z, -1);
  assertEq(j.w, 0);

  var k = int32x4.bool(true, true, true, true, true);
  assertEq(k.x, -1);
  assertEq(k.y, -1);
  assertEq(k.z, -1);
  assertEq(k.w, -1);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
