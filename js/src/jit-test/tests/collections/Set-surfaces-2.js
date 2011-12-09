// Set methods throw when passed a this-value that isn't a Set.

load(libdir + "asserts.js");

function testcase(obj, fn) {
    assertEq(typeof fn, "function");
    var args = Array.slice(arguments, 2);
    assertThrowsInstanceOf(function () { fn.apply(obj, args); }, TypeError);
}

function test(obj) {
    testcase(obj, Set.prototype.has, 12);
    testcase(obj, Set.prototype.add, 12);
    testcase(obj, Set.prototype.delete, 12);
}

test(Set.prototype);
test(Object.create(new Set));
test(new Map());
test({});
test(null);
test(undefined);
