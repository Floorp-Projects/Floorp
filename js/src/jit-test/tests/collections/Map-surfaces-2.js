// Map methods throw when passed a this-value that isn't a Map.

load(libdir + "asserts.js");

function testcase(obj, fn) {
    assertEq(typeof fn, "function");
    var args = Array.slice(arguments, 2);
    assertThrowsInstanceOf(function () { fn.apply(obj, args); }, TypeError);
}

function test(obj) {
    testcase(obj, Map.prototype.get, "x");
    testcase(obj, Map.prototype.has, "x");
    testcase(obj, Map.prototype.set, "x", 1);
    testcase(obj, Map.prototype.delete, "x");
}

test(Map.prototype);
test(Object.create(new Map));
test(new Set());
test({});
test(null);
test(undefined);
