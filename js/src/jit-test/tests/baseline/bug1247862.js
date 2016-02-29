var T = TypedObject;
ValueStruct = new T.StructType({
    f: T.int32,
    g: T.Any
});
var v = new ValueStruct;
for (var i = 0; i < 2; i++) {
    var a = {};
    var b = v.f = 3;
    var c = v.g = a;
    assertEq(b === 3, true);
    assertEq(c === a, true);
}
