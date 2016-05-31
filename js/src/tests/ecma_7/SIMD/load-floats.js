// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var { testLoad } = Helpers;

testLoad('Float32x4', new Float32Array(SIZE_32_ARRAY));
testLoad('Float64x2', new Float64Array(SIZE_64_ARRAY));

if (typeof SharedArrayBuffer != "undefined") {
  testLoad('Float32x4', new Float32Array(new SharedArrayBuffer(SIZE_8_ARRAY)));
  testLoad('Float64x2', new Float64Array(new SharedArrayBuffer(SIZE_8_ARRAY)));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
