// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// Check that NaN normalization is applied when extracting the x lane
// out, after bit conversion has occurred.

var Int32x4 = SIMD.Int32x4;
var a = Int32x4((4294967295), 200, 300, 400);
var c = SIMD.Float32x4.fromInt32x4Bits(a);

// NaN canonicalization occurs when extracting out x lane:
assertEq(c.x, NaN);

// but underlying bits are faithfully transmitted
// (though reinterpreted as a signed integer):
var d = SIMD.Int32x4.fromFloat32x4Bits(c);
assertEq(d.x, -1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
