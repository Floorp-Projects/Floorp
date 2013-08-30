// |reftest| skip-if(!this.hasOwnProperty("Type"))
var BUGNUMBER = 578700;
var summary = 'BinaryData StructType propery enumeration';

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

