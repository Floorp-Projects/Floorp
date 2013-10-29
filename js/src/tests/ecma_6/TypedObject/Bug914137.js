// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 914137;
var summary = 'Fuzz bug';

function test() {
  var A = new TypedObject.ArrayType(TypedObject.uint8, 2147483647);
  var a = new A();
  assertEq(arguments[5], a);
}

print(BUGNUMBER + ": " + summary);
assertThrows(test);
if (typeof reportCompare === "function")
  reportCompare(true, true);
print("Tests complete");
