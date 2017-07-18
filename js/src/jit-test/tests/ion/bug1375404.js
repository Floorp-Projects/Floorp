// |jit-test| error:ReferenceError

Object.prototype.f = 42;
var T = TypedObject;
var ObjectStruct = new T.StructType({f: T.Object});
Object.prototype.f = 42;
var o = new ObjectStruct();
evaluate(`
  o.f %= p;
`);
