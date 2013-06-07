// Set methods throw when passed a this-value that isn't a Set.

load(libdir + "asserts.js");

function testcase(obj, fn) {
    assertEq(typeof fn, "function");
    var args = Array.slice(arguments, 2);
    assertThrowsInstanceOf(function () { fn.apply(obj, args); }, TypeError);
}

var Set_size_getter = Object.getOwnPropertyDescriptor(Set.prototype, "size").get;

function test(obj) {
    testcase(obj, Set.prototype.has, 12);
    testcase(obj, Set.prototype.add, 12);
    testcase(obj, Set.prototype.delete, 12);
    testcase(obj, Set.prototype.clear);
    testcase(obj, Set.prototype.keys);
    testcase(obj, Set.prototype.values);
    testcase(obj, Set.prototype.entries);
    testcase(obj, Set_size_getter);
}

test(Set.prototype);
test(Object.create(new Set));
test(new Map());
test({});
test(null);
test(undefined);
