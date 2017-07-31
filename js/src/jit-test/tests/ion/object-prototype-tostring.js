var toString = Object.prototype.toString;
var iter = 500;

function testConstant() {
    for (var i = 0; i < iter; i++) {
        assertEq(({}).toString(), "[object Object]");
        assertEq(toString.call([]), "[object Array]");
        assertEq(toString.call(Math.abs), "[object Function]");
    }
}
testConstant();

function testOwnToStringTag() {
    var stringify = o => toString.call(o);
    var o = {};
    for (var i = 0; i < iter; i++)
        assertEq(stringify(o), "[object Object]");
    o[Symbol.toStringTag] = "foo";
    for (var i = 0; i < iter; i++)
        assertEq(stringify(o), "[object foo]");
}
testOwnToStringTag();

function testDynamic() {
    var arr = [{}, [], new Date, /a/];
    var expected = ["[object Object]", "[object Array]", "[object Date]", "[object RegExp]"];
    for (var i = 0; i < iter; i++) {
        for (var j = 0; j < arr.length; j++)
            assertEq(toString.call(arr[j]), expected[j]);
    }
}
testDynamic();

function testToStringTagProto() {
    var c = 0;
    Object.defineProperty(Date.prototype, Symbol.toStringTag, {get() { c++; return "evil"; }});
    var arr = [{}, [], new Date, /a/];
    var expected = ["[object Object]", "[object Array]", "[object evil]", "[object RegExp]"];
    for (var i = 0; i < iter; i++) {
        for (var j = 0; j < arr.length; j++)
            assertEq(toString.call(arr[j]), expected[j]);
    }
    assertEq(c, iter);
}
testToStringTagProto();
