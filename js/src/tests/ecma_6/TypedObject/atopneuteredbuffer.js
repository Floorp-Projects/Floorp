// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 976697;

var {StructType, uint32, Object, Any, storage, objectType} = TypedObject;

function main() { // once a C programmer, always a C programmer.
  print(BUGNUMBER + ": " + summary);

  var Uints = uint32.array();
  var Unit = new StructType({});   // Empty struct type
  var buffer = new ArrayBuffer(0); // Empty buffer
  var p = new Unit(buffer);        // OK
  neuter(buffer);
  assertThrowsInstanceOf(() => new Unit(buffer), TypeError,
                         "Able to instantiate atop neutered buffer");
  assertThrowsInstanceOf(() => new Uints(buffer, 0), TypeError,
                         "Able to instantiate atop neutered buffer");

  reportCompare(true, true);
  print("Tests complete");
}

main();
