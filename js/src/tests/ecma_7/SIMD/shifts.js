// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var int32x4 = SIMD.int32x4;

function lsh(a, b) {
    return (a << b) | 0;
}
function rsh(a, b) {
    return (a >> b) | 0;
}
function ursh(a, b) {
    return (a >>> b) | 0;
}

function test() {
  for (var v of [
            int32x4(-1, 2, -3, 4),
            int32x4(INT32_MAX, INT32_MIN, INT32_MAX - 1, INT32_MIN + 1)
       ])
  {
      for (var bits = 0; bits < 32; bits++) {
          testBinaryScalarFunc(v, bits, int32x4.shiftLeftByScalar, lsh);
          testBinaryScalarFunc(v, bits, int32x4.shiftRightArithmeticByScalar, rsh);
          testBinaryScalarFunc(v, bits, int32x4.shiftRightLogicalByScalar, ursh);
      }
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
