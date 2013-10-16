// |jit-test| error:Error

if (!this.hasOwnProperty("TypedObject"))
  throw new Error("type too large");

var AA = new TypedObject.ArrayType(new ArrayType(TypedObject.uint8, 2147483647), 5);
var aa = new AA();
var aa0 = aa[0];
