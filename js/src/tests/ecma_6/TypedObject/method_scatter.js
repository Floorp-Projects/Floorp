// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 939715;
var summary = 'method instance.scatter';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;
var uint16 = TypedObject.uint16;
var uint32 = TypedObject.uint32;
var uint8Clamped = TypedObject.uint8Clamped;
var int8 = TypedObject.int8;
var int16 = TypedObject.int16;
var int32 = TypedObject.int32;
var float32 = TypedObject.float32;
var float64 = TypedObject.float64;

function scatterUint8sPermute() {
  var uint8Array = uint8.array(5);
  var array = new uint8Array([124, 120, 122, 123, 121]);

  var perm = array.scatter(uint8Array, [4, 0, 2, 3, 1]);
  assertTypedEqual(uint8Array, perm, [120, 121, 122, 123, 124]);
}

function scatterUint8sPermuteIncomplete() {
  var uint8Array4 = uint8.array(4);
  var uint8Array5 = uint8.array(5);
  var array = new uint8Array4([124, 120, 122, 123]);

  var perm;
  perm = array.scatter(uint8Array5, [4, 0, 2, 3]);
  assertTypedEqual(uint8Array5, perm, [120,  0, 122, 123, 124]);

  perm = array.scatter(uint8Array5, [4, 0, 2, 3], 77);
  assertTypedEqual(uint8Array5, perm, [120, 77, 122, 123, 124]);
}

function scatterUint8sHistogram() {
  var uint32Array5 = uint32.array(5);
  var uint32Array3 = uint32.array(3);
  var array = new uint32Array5([1, 10, 100, 1000, 10000]);

  var hist = array.scatter(uint32Array3, [1, 1, 2, 1, 0], 0, (a,b) => a+b);
  assertTypedEqual(uint32Array3, hist, [10000, 1011, 100]);
}

function scatterUint8sCollisionThrows() {
  var uint32Array5 = uint32.array(5);
  var uint32Array3 = uint32.array(3);
  var array = new uint32Array5([1, 10, 100, 1000, 10000]);

  var unset_nonce = new Object();
  var unset = unset_nonce;
  try {
    unset = array.scatter(uint32Array3, [1, 1, 2, 1, 0], 0);
  } catch (e) {
    assertEq(unset, unset_nonce);
  }
}

function scatterUint8sConflictIsAssocNonCommute() {
  var uint32Array5 = uint32.array(5);
  var uint32Array3 = uint32.array(3);
  var array = new uint32Array5([1, 10, 100, 1000, 10000]);

  // FIXME strawman spec says conflict must be associative, but does
  // not dictate commutative.  Yet, strawman spec does not appear to
  // specify operation order; must address incongruence.

  var lfts = array.scatter(uint32Array3, [1, 1, 2, 1, 0], 0, (a,b) => a);
  assertTypedEqual(uint32Array3, lfts, [10000, 1, 100]);
  var rgts = array.scatter(uint32Array3, [1, 1, 2, 1, 0], 0, (a,b) => b);
  assertTypedEqual(uint32Array3, rgts, [10000, 1000, 100]);
}

function runTests() {
    print(BUGNUMBER + ": " + summary);

    scatterUint8sPermute();
    scatterUint8sPermuteIncomplete();
    scatterUint8sHistogram();
    scatterUint8sCollisionThrows();
    scatterUint8sConflictIsAssocNonCommute();

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
