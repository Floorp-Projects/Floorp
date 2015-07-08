// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var int8x16 = SIMD.int8x16;

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  var a = int8x16.bool(true, false, true, false, true, true, false, false, true, true, true, false, false, false, true, true);
  assertEq(a.s0, -1);
  assertEq(a.s1, 0);
  assertEq(a.s2, -1);
  assertEq(a.s3, 0);
  assertEq(a.s4, -1);
  assertEq(a.s5, -1);
  assertEq(a.s6, 0);
  assertEq(a.s7, 0);
  assertEq(a.s8, -1);
  assertEq(a.s9, -1);
  assertEq(a.s10, -1);
  assertEq(a.s11, 0);
  assertEq(a.s12, 0);
  assertEq(a.s13, 0);
  assertEq(a.s14, -1);
  assertEq(a.s15, -1);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
