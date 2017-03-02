// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 898356;
var summary = "";

var {StructType, uint32, Object, Any, storage, objectType} = TypedObject;

function main() { // once a C programmer, always a C programmer.
  print(BUGNUMBER + ": " + summary);

  var Uints = new StructType({f: uint32, g: uint32});

  var anArray = new Uint32Array(2);
  anArray[0] = 22;
  anArray[1] = 44;

  var uints = new Uints(anArray.buffer);
  assertEq(storage(uints).buffer, anArray.buffer);
  assertEq(uints.f, 22);
  assertEq(uints.g, 44);

  uints.f++;
  assertEq(anArray[0], 23);

  reportCompare(true, true);
  print("Tests complete");
}

main();
