// Fuzz bug 981650: Test creating an unsized array type based on an instance of
// that same type.

if (typeof TypedObject === "undefined")
  quit();

var T = TypedObject;
var AT = new T.ArrayType(T.int32);
var v = new AT(10);
new AT(v);
