// |jit-test| error:Error

if (!this.hasOwnProperty("Type"))
  throw new Error("type too large");

var AA = new ArrayType(new ArrayType(uint8, (2147483647)), 5);
var aa = new AA();
var aa0 = aa[0];
