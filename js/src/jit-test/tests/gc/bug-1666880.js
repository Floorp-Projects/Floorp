// |jit-test| error: ReferenceError

var T84 = TypedObject;
ValueStruct = new T84.StructType({
    g89: T84.Any
});
var v82 = new ValueStruct;
a20 = BigInt(-1);
var c18 = v82.g89 = a20;
gczeal(2);
if (line == null) {}
