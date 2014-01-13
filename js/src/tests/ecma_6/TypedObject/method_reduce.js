// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 939715;
var summary = 'method instance.reduce';

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

function reduceUint8s() {
  var uint8Array = uint8.array(5);
  var array = new uint8Array([128, 129, 130, 131, 132]);

  var sum = array.reduce((a, b) => a + b);
  assertEq(sum, (128+129+130+131+132) % 256);

  var f64Array = float64.array(5);
  var floats = new f64Array([128.0, 129.0, 130.0, 131.0, 132.0]);

  // (Note that floating point add is not associative in general;
  //  we should double-check that the result below is robust.)
  var fsum = floats.reduce((a, b) => a + b);
  assertEq(fsum, 128.0+129.0+130.0+131.0+132.0);
}

function reduceVectors() {
  var VectorType = uint32.array(3);
  var VectorsType = VectorType.array(3);
  var array = new VectorsType([[1, 2, 3],
                               [4, 5, 6],
                               [7, 8, 9]]);

  var sum = array.reduce(vectorAdd);
  assertTypedEqual(VectorType,
                   sum,
                   new VectorType([1+4+7,
                                   2+5+8,
                                   3+6+9]));

  // The mutated accumulator does not alias the input.
  assertTypedEqual(VectorsType,
                   array,
                   new VectorsType([[1, 2, 3],
                                    [4, 5, 6],
                                    [7, 8, 9]]));

  var sum = array.reduce(vectorAddFunctional);
  assertTypedEqual(VectorType,
                   sum,
                   new VectorType([1+4+7,
                                   2+5+8,
                                   3+6+9]));

  function vectorAdd(l, r) {
    assertEq(l.length, r.length);
    for (var i = 0; i < l.length; i++)
      l[i] += r[i];
    return l;
  }

  function vectorAddFunctional(l, r) {
    assertEq(l.length, r.length);
    return VectorType.build(1, i => l[i] + r[i]);
  }

}

function runTests() {
    print(BUGNUMBER + ": " + summary);

    reduceUint8s();
    reduceVectors();

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
