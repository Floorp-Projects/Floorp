// API Surface Test: check that mutating prototypes
// of type descriptors has no effect.

if (!this.hasOwnProperty("TypedObject"))
  quit();

var {StructType, uint32, Object, Any, storage, objectType} = TypedObject;

function main() { // once a C programmer, always a C programmer.
  var Uints = new StructType({f: uint32, g: uint32});
  var p = Uints.prototype;
  Uints.prototype = {}; // no effect
  assertEq(p, Uints.prototype);

  var Uintss = Uints.array(2);
  var p = Uintss.prototype;
  Uintss.prototype = {}; // no effect
  assertEq(p, Uintss.prototype);

  print("Tests complete");
}

main();
