// Bug 976697. Check for various quirks when instantiating a typed
// object atop an already neutered buffer.

if (typeof TypedObject === "undefined")
  quit();

load(libdir + "asserts.js")

var {StructType, uint32, Object, Any, storage, objectType} = TypedObject;

function main(variant) { // once a C programmer, always a C programmer.
  var Uints = uint32.array(0);
  var Unit = new StructType({});   // Empty struct type
  var buffer = new ArrayBuffer(0); // Empty buffer
  var p = new Unit(buffer);        // OK
  neuter(buffer, variant);
  assertThrowsInstanceOf(() => new Unit(buffer), TypeError,
                         "Able to instantiate atop neutered buffer");
  assertThrowsInstanceOf(() => new Uints(buffer), TypeError,
                         "Able to instantiate atop neutered buffer");
}

main("same-data");
main("change-data");
