if (typeof TypedObject === "undefined")
    quit();

var T = TypedObject;

var ObjectStruct = new T.StructType({f: T.Object});
var StringStruct = new T.StructType({f: T.string});
var ValueStruct = new T.StructType({f: T.Any});

// Suppress ion compilation of the global script.
with({}){}

var o = new ObjectStruct();
var s = new StringStruct();
var v = new ValueStruct();

// Make sure that unboxed null pointers on the stack are marked correctly.
whatever = new Array();
function testGC(o, p) {
    for (var i = 0; i < 5; i++) {
        minorgc();
        o.f = p;
        whatever.push(new Array()); // minorgc needs garbage before it scans the stack.
    }
}
testGC(o, {});
testGC(o, null);

// Test writing various things to an object field.
function writeObject(o, v, expected) {
    o.f = v;
    assertEq(typeof o.f, "object");
    assertEq("" + o.f, expected);
}
for (var i = 0; i < 5; i++)
    writeObject(o, {toString: function() { return "helo"} }, "helo");
for (var i = 0; i < 5; i++)
    writeObject(o, null, "null");
for (var i = 0; i < 5; i++)
    writeObject(o, "three", "three");
for (var i = 0; i < 5; i++)
    writeObject(o, 4.5, "4.5");
for (var i = 0; i < 5; i++) {
    try {
        writeObject(o, undefined, "");
    } catch (e) {
        assertEq(e instanceof TypeError, true);
    }
}

// Test writing various things to a string field.
function writeString(o, v, expected) {
    o.f = v;
    assertEq(typeof o.f, "string");
    assertEq("" + o.f, expected);
}
for (var i = 0; i < 5; i++)
    writeString(s, {toString: function() { return "helo"} }, "helo");
for (var i = 0; i < 5; i++)
    writeString(s, null, "null");
for (var i = 0; i < 5; i++)
    writeString(s, "three", "three");
for (var i = 0; i < 5; i++)
    writeString(s, 4.5, "4.5");
for (var i = 0; i < 5; i++)
    writeString(s, undefined, "undefined");

// Test writing various things to a value field.
function writeValue(o, v, expectedType, expected) {
    o.f = v;
    assertEq(typeof o.f, expectedType);
    assertEq("" + o.f, expected);
}
for (var i = 0; i < 5; i++)
    writeValue(v, {toString: function() { return "helo"} }, "object", "helo");
for (var i = 0; i < 5; i++)
    writeValue(v, null, "object", "null");
for (var i = 0; i < 5; i++)
    writeValue(v, "three", "string", "three");
for (var i = 0; i < 5; i++)
    writeValue(v, 4.5, "number", "4.5");
for (var i = 0; i < 5; i++)
    writeValue(v, undefined, "undefined", "undefined");
