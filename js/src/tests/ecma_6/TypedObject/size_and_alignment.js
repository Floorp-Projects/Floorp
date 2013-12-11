// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 578700;
var summary = 'Size and Alignment of TypedObjects types';
var actual = '';
var expect = '';


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


function runTests() {
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  var typesAndAlignments = [
    {type: uint8, size: 1, alignment: 1},
    {type: uint8Clamped, size: 1, alignment: 1},
    {type: uint16, size: 2, alignment: 2},
    {type: uint32, size: 4, alignment: 4},

    {type: int8, size: 1, alignment: 1},
    {type: int16, size: 2, alignment: 2},
    {type: int32, size: 4, alignment: 4},

    {type: float32, size: 4, alignment: 4},
    {type: float64, size: 8, alignment: 8},

    {type: new StructType({a: uint8, b: uint16, c: uint8}), size: 6, alignment: 2},

    {type: new StructType({a: uint8, b: uint8, c: uint16}), size: 4, alignment: 2},

    {type: new ArrayType(uint8).dimension(32), size: 32, alignment: 1},
    {type: new ArrayType(uint16).dimension(16), size: 32, alignment: 2},
    {type: new ArrayType(uint32).dimension(8), size: 32, alignment: 4},
  ];

  for (var i = 0; i < typesAndAlignments.length; i++) {
    var test = typesAndAlignments[i];
    print("Type:", test.type.toSource(),
          "Size:", test.type.byteLength,
          "Alignment:", test.type.byteAlignment);
    assertEq(test.type.byteLength, test.size);
    assertEq(test.type.byteAlignment, test.alignment);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
  print("Tests complete");
}

runTests();
