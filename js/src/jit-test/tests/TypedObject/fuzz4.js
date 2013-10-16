// |jit-test| error:Error;

if (!this.hasOwnProperty("TypedObject"))
  throw new Error();

var A = new TypedObject.ArrayType(TypedObject.uint8, 10);
var a = new A();
a.forEach(function(val, i) {
  assertEq(arguments[5], a);
});
