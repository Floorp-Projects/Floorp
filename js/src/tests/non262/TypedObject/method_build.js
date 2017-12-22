// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 939715;
var summary = 'method type.build';

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

function oneDimensionalArrayOfUints() {
  var grain = uint32;
  var type = grain.array(4);
  var r1 = type.build(x => x * 2);
  assertTypedEqual(type, r1, new type([0, 2, 4, 6]));
}

function oneDimensionalArrayOfStructs() {
  var grain = new StructType({f: uint32});
  var type = grain.array(4);
  var r1 = type.build(x => new grain({f: x * 2}));
  var r2 = type.build((x, out) => { out.f = x * 2; });
  assertTypedEqual(type, r1, new type([{f:0}, {f:2},
                                       {f:4}, {f:6}]));
  assertTypedEqual(type, r1, r2);
}

// At an attempt at readability, the tests below all try to build up
// numbers where there is a one-to-one mapping between input dimension
// and base-10 digit in the output.
//
// (Note that leading zeros must be elided in the expected-values to
// avoid inadvertantly interpreting the numbers as octal constants.)

function twoDimensionalArrayOfStructsWithDepth2() {
  var grain = new StructType({f: uint32});
  var type = grain.array(2, 2);

  var r1 = type.build(2, (x, y) => {
    return new grain({f: x * 10 + y});
  });

  var r2 = type.build(2, (x, y, out) => {
    out.f = x * 10 + y;
  });

  assertTypedEqual(type, r1, new type([[{f: 0}, {f: 1}],
                                       [{f:10}, {f:11}]]));
  assertTypedEqual(type, r1, r2);
}

function twoDimensionalArrayOfStructsWithDepth1() {
  var grain = new StructType({f: uint32}).array(2);
  var type = grain.array(2);

  var r1 = type.build((x) => {
    return new grain([{f: x * 10},
                      {f: x * 10 + 1}]);
  });

  var r2 = type.build(1, (x, out) => {
    out[0].f = x * 10 + 0;
    out[1].f = x * 10 + 1;
  });

  assertTypedEqual(type, r1, new type([[{f: 0}, {f: 1}],
                                       [{f:10}, {f:11}]]));
  assertTypedEqual(type, r1, r2);
}

function threeDimensionalArrayOfUintsWithDepth3() {
  var grain = uint32;
  var type = grain.array(2).array(2).array(2);
  var r1 = type.build(3, (x,y,z) => x * 100 + y * 10 + z);
  assertTypedEqual(type, r1, new type([[[  0,   1], [ 10,  11]],
                                       [[100, 101], [110, 111]]]));
}

function threeDimensionalArrayOfUintsWithDepth2() {
  var grain = uint32.array(2);
  var type = grain.array(2).array(2);
  var r1 = type.build(2, (x,y) => [x * 100 + y * 10 + 0, x * 100 + y * 10 + 1]);
  var r1b = type.build(2, (x,y) => grain.build(z => x * 100 + y * 10 + z));
  var r1c = type.build(2, (x,y) => grain.build(1, z => x * 100 + y * 10 + z));

  var r2 = type.build(2, (x,y, out) => { out[0] = x * 100 + y * 10 + 0;
                                         out[1] = x * 100 + y * 10 + 1;
                                       });
  assertTypedEqual(type, r1, new type([[[  0,   1], [ 10,  11]],
                                       [[100, 101], [110, 111]]]));
  assertTypedEqual(type, r1, r1b);
  assertTypedEqual(type, r1, r1c);
  assertTypedEqual(type, r1, r2);
}

function threeDimensionalArrayOfUintsWithDepth1() {
  var grain = uint32.array(2).array(2);
  var type = grain.array(2);
  var r1 = type.build(1, (x) => grain.build(y => [x * 100 + y * 10 + 0, x * 100 + y * 10 + 1]));
  var r1b = type.build(1, (x) => grain.build(1, y => [x * 100 + y * 10 + 0, x * 100 + y * 10 + 1]));
  var r1c = type.build(1, (x) => grain.build(2, (y,z) => x * 100 + y * 10 + z));
  var r2 = type.build(1, (x, out) => { out[0][0] = x * 100 + 0 * 10 + 0;
                                       out[0][1] = x * 100 + 0 * 10 + 1;
                                       out[1][0] = x * 100 + 1 * 10 + 0;
                                       out[1][1] = x * 100 + 1 * 10 + 1;
                                     });
  assertTypedEqual(type, r1, new type([[[  0,   1], [ 10,  11]],
                                       [[100, 101], [110, 111]]]));
  assertTypedEqual(type, r1, r1b);
  assertTypedEqual(type, r1, r1c);
  assertTypedEqual(type, r1, r2);
}

function runTests() {
    print(BUGNUMBER + ": " + summary);

    oneDimensionalArrayOfUints();
    oneDimensionalArrayOfStructs();
    twoDimensionalArrayOfStructsWithDepth2();
    twoDimensionalArrayOfStructsWithDepth1();
    threeDimensionalArrayOfUintsWithDepth3();
    threeDimensionalArrayOfUintsWithDepth2();
    threeDimensionalArrayOfUintsWithDepth1();

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
