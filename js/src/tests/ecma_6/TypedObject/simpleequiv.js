// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 922216;
var summary = 'TypedObjects Equivalent Numeric Types';

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
  print(BUGNUMBER + ": " + summary);

  var scalarTypes = [
    int8, int16, int32,
    uint8, uint16, uint32,
    float32, float64
  ];

  for (var i = 0; i < scalarTypes.length; i++)
    for (var j = 0; j < scalarTypes.length; j++)
      assertEq(i == j, scalarTypes[i].equivalent(scalarTypes[j]));

  reportCompare(true, true);
  print("Tests complete");
}

runTests();
