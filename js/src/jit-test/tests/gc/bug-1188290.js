load(libdir + "immutable-prototype.js");

if (globalPrototypeChainIsMutable())
    this.__proto__ = [];

var T = TypedObject;
var ObjectStruct = new T.StructType({f: T.Object});
var o = new ObjectStruct();

minorgc();

function writeObject(o, v) {
    o.f = v;
    assertEq(typeof o.f, "object");
}

for (var i = 0; i < 5; i++)
    writeObject(o, { toString: function() { return "helo"; } });
