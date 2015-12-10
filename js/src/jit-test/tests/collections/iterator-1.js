// collection.iterator() returns an iterator object.

load(libdir + "iteration.js");

function test(obj, name) {
    var iter = obj[Symbol.iterator]();
    assertEq(typeof iter, "object");
    assertEq(iter instanceof Iterator, false); // Not a legacy Iterator.
    assertEq(iter.toString(), "[object " + obj.constructor.name + " Iterator]");
}

test([]);
test(new Map);
test(new Set);
