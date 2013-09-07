// |jit-test| error:Error

if (!this.hasOwnProperty("Type"))
  throw new Error("type too large");

var A = new ArrayType(uint8, (2147483647));
var S = new StructType({a: A,
                        b: A,
                        c: A,
                        d: A,
                        e: A});
var aa = new S();
var aa0 = aa.a;
