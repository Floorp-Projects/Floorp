// |jit-test| error:Error;

if (!this.hasOwnProperty("Type"))
  throw new Error();

var A = new ArrayType(uint8, 10);
var a = new A();
a.forEach(function(val, i) {
  assertEq(arguments[5], a);
});
