if (typeof TypedObject === "undefined")
  quit();

var T = TypedObject;
var ObjectStruct = new T.StructType({f: T.Object});
var o = new ObjectStruct();
function testGC(o, p) {
    for (var i = 0; i < 5; i++) {
        minorgc();
        o.f >>=  p;
    }
}
testGC(o, {});
