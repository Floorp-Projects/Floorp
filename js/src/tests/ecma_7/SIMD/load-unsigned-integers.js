// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var { testLoad } = Helpers;

testLoad('Uint8x16', new Uint8Array(SIZE_8_ARRAY));
testLoad('Uint16x8', new Uint16Array(SIZE_16_ARRAY));
testLoad('Uint32x4', new Uint32Array(SIZE_32_ARRAY));

if (typeof SharedArrayBuffer != "undefined") {
  testLoad('Uint8x16', new Uint8Array(new SharedArrayBuffer(SIZE_8_ARRAY)));
  testLoad('Uint16x8', new Uint16Array(new SharedArrayBuffer(SIZE_8_ARRAY)));
  testLoad('Uint32x4', new Uint32Array(new SharedArrayBuffer(SIZE_8_ARRAY)));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);

