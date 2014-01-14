// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 953270;
var summary = 'Handles';

// Check that NaN normalization is applied when extracting the x lane
// out, after bit conversion has occurred.

var int32x4 = SIMD.int32x4;
var a = int32x4((4294967295), 200, 300, 400);
var c = SIMD.int32x4.bitsToFloat32x4(a);

// NaN canonicalization occurs when extracting out x lane:
assertEq(c.x, NaN);

// but underlying bits are faithfully transmitted
// (though reinterpreted as a signed integer):
var d = SIMD.float32x4.bitsToInt32x4(c);
assertEq(d.x, -1);

reportCompare(true, true);
print("Tests complete");
