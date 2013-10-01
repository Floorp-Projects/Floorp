// collection.iterator() returns an Iterator object.

function test(obj, name) {
    var iter = obj.iterator();
    assertEq(typeof iter, "object");
    assertEq(iter instanceof Iterator, true);
    assertEq(iter.toString(), "[object " + obj.constructor.name + " Iterator]");
}

test([]);
test(new Map);
test(new Set);
