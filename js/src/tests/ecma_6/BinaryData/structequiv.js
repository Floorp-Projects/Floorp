// |reftest| skip-if(!this.hasOwnProperty("Type"))
var BUGNUMBER = 578700;
var summary = 'BinaryData Equivalent StructTypes';

function assertEquivalent(t1, t2) {
  assertEq(true, t1.equivalent(t2));
  assertEq(true, t2.equivalent(t1));
}

function assertNotEquivalent(t1, t2) {
  assertEq(false, t1.equivalent(t2));
  assertEq(false, t2.equivalent(t1));
}

function runTests() {
  print(BUGNUMBER + ": " + summary);

  // Create a line:
  var PixelType1 = new StructType({x: uint8, y: uint8});
  var LineType1 = new StructType({from: PixelType1, to: PixelType1});

  // Sanity checks about type equivalence:
  assertEquivalent(PixelType1, PixelType1);
  assertEquivalent(LineType1, LineType1);
  assertNotEquivalent(PixelType1, LineType1);

  // Define the same two types again. Equivalent.
  var PixelType2 = new StructType({x: uint8, y: uint8});
  var LineType2 = new StructType({from: PixelType2, to: PixelType2});
  assertEquivalent(PixelType1, PixelType2);
  assertEquivalent(LineType1, LineType2);

  // Define the pixel type with field order reversed. Not equivalent.
  var PixelType3 = new StructType({y: uint8, x: uint8});
  var LineType3 = new StructType({from: PixelType3, to: PixelType3});
  assertNotEquivalent(PixelType1, PixelType3);
  assertNotEquivalent(LineType1, LineType3);

  // Define the line type with field order reversed. Not equivalent.
  var PixelType4 = new StructType({x: uint8, y: uint8});
  var LineType4 = new StructType({to: PixelType4, from: PixelType4});
  assertEquivalent(PixelType1, PixelType4);
  assertNotEquivalent(LineType1, LineType4);

  reportCompare(true, true);
  print("Tests complete");
}

runTests();
