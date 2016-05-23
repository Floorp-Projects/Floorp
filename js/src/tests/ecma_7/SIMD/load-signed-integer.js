// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var { testLoad } = Helpers;

testLoad('Int8x16', new Int8Array(SIZE_8_ARRAY));
testLoad('Int16x8', new Int16Array(SIZE_16_ARRAY));
testLoad('Int32x4', new Int32Array(SIZE_32_ARRAY));

if (typeof SharedArrayBuffer != "undefined") {
  testLoad('Int8x16', new Int8Array(new SharedArrayBuffer(SIZE_8_ARRAY)));
  testLoad('Int16x8', new Int16Array(new SharedArrayBuffer(SIZE_8_ARRAY)));
  testLoad('Int32x4', new Int32Array(new SharedArrayBuffer(SIZE_8_ARRAY)));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);

