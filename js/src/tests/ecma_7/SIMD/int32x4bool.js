// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Int32x4 = SIMD.Int32x4;

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
  assertEqX4(Int32x4.bool(true, false, true, false), [-1, 0, -1, 0]);
  assertEqX4(Int32x4.bool(5, 0, 1, -2), [-1, 0, -1, -1]);
  assertEqX4(Int32x4.bool(1.23, 13.37, 42.99999999, 0.000001), [-1, -1, -1, -1]);
  assertEqX4(Int32x4.bool("string", "", "1", "0"), [-1, 0, -1, -1]);
  assertEqX4(Int32x4.bool([], [1, 2, 3], SIMD.Int32x4(1, 2, 3, 4), function() { return 0; }), [-1, -1, -1, -1]);
  assertEqX4(Int32x4.bool(undefined, null, {}, tryEmulateUndefined()), [0, 0, -1, 0]);

  // Test missing arguments
  assertEqX4(Int32x4.bool(), [0, 0, 0, 0]);
  assertEqX4(Int32x4.bool(true), [-1, 0, 0, 0]);
  assertEqX4(Int32x4.bool(true, true), [-1, -1, 0, 0]);
  assertEqX4(Int32x4.bool(true, true, true), [-1, -1, -1, 0]);
  assertEqX4(Int32x4.bool(true, true, true, true), [-1, -1, -1, -1]);
  assertEqX4(Int32x4.bool(true, true, true, true, true), [-1, -1, -1, -1]);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
