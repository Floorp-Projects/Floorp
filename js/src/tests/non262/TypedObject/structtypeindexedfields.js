// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 578700;
var summary = 'TypedObjects: indexed properties are illegal in a StructType';

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

  var failed;
  try {
    new StructType({1: int32, 2: uint8, 3: float64});
    failed = false;
  } catch (e) {
    failed = true;
  }
  reportCompare(failed, true);
  print("Tests complete");
}

runTests();
