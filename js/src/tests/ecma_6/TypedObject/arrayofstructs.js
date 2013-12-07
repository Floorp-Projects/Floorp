// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 578700;
var summary = 'TypedObjects StructType prototype chains';

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var float32 = TypedObject.float32;

function runTests() {
  var Point = new ArrayType(float32).dimension(3);
  var Line = new StructType({from: Point, to: Point});
  var Lines = new ArrayType(Line).dimension(3);

  var lines = new Lines([
    {from: [1, 2, 3], to: [4, 5, 6]},
    {from: [7, 8, 9], to: [10, 11, 12]},
    {from: [13, 14, 15], to: [16, 17, 18]}
  ]);

  assertEq(lines[1].to[1], 11);
  assertEqArray(lines[2].from, [13, 14, 15]);

  reportCompare(true, true);
  print("Tests complete");
}

runTests();


