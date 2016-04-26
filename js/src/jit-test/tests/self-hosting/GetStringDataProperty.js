// Bug 1267364 - GetStringDataProperty should return undefined when the object
// is non-native.

var GetStringDataProperty = getSelfHostedValue("GetStringDataProperty");

function testProxy() {
    var obj = new Proxy({"foo": "10"}, {});
    var v = GetStringDataProperty(obj, "foo");
    assertEq(v, undefined);
}

function testMaybeUnboxed() {
    // Use JSON.parse to create unboxed object if availbale.
    var obj = JSON.parse("[" + '{"foo": "10"},'.repeat(100) +"{}]");

    // GetStringDataProperty may return "10" or undefined, depending on whether
    // `obj` is unboxed or not
    var v = GetStringDataProperty(obj[0], "foo");
    assertEq(v == undefined || v == "10", true);
}

function testTypedObject() {
    var {StructType, string} = TypedObject;
    var S = new StructType({foo: string});
    var obj = new S({foo: "10"});
    var v = GetStringDataProperty(obj, "foo");
    assertEq(v, undefined);
}

testProxy();
testMaybeUnboxed();
if (typeof TypedObject !== "undefined")
    testTypedObject();
