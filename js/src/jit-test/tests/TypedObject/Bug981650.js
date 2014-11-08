// Fuzz bug 981650: Test creating an array type based on an instance of
// that same type.

if (typeof TypedObject === "undefined")
  quit();

var T = TypedObject;
var v = new T.ArrayType(T.int32, 10);
new v(v);
