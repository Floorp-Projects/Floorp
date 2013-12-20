// |jit-test| error:Error

if (!this.hasOwnProperty("TypedObject"))
  throw new Error("type too large");

var A = TypedObject.uint8.array(2147483647);
var S = new TypedObject.StructType({a: A,
                                    b: A,
                                    c: A,
                                    d: A,
                                    e: A});
var aa = new S();
var aa0 = aa.a;
