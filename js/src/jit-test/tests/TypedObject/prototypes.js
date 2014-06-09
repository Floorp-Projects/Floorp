// API Surface Test: check that mutating prototypes
// of type objects has no effect, and that mutating
// the prototypes of typed objects is an error.

if (!this.hasOwnProperty("TypedObject"))
  quit();

load(libdir + "asserts.js");

var {StructType, uint32, Object, Any, storage, objectType} = TypedObject;

function main() { // once a C programmer, always a C programmer.
  var Uints = new StructType({f: uint32, g: uint32});
  var p = Uints.prototype;
  Uints.prototype = {}; // no effect
  assertEq(p, Uints.prototype);

  var uints = new Uints();
  assertEq(uints.__proto__, p);
  assertThrowsInstanceOf(function() uints.__proto__ = {},
                         TypeError);
  assertThrowsInstanceOf(function() Object.setPrototypeOf(uints, {}),
                         TypeError);
  assertEq(uints.__proto__, p);

  var Uintss = Uints.array(2);
  var p = Uintss.prototype;
  Uintss.prototype = {}; // no effect
  assertEq(p, Uintss.prototype);

  print("Tests complete");
}

main();
