// |reftest| skip-if(!this.hasOwnProperty("Type"))
var BUGNUMBER = 578700;
var summary = 'BinaryData: indexed properties are illegal in a StructType';

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
