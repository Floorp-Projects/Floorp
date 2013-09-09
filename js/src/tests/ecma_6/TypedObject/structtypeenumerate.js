// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 578700;
var summary = 'TypedObjects StructType propery enumeration';

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
  var RgbColor = new StructType({r: uint8, g: uint8, b: uint8});
  var Fade = new StructType({from: RgbColor, to: RgbColor});

  var white = new RgbColor({r: 255, g: 255, b: 255});
  var gray = new RgbColor({r: 129, g: 128, b: 127});
  var fade = new Fade({from: white, to: gray});

  var keys = Object.keys(gray);
  assertEqArray(keys, ["r", "g", "b"]);

  var keys = Object.keys(fade);
  assertEqArray(keys, ["from", "to"]);

  reportCompare(true, true);
  print("Tests complete");
}

runTests();

