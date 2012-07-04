// collection.iterator() returns an Iterator object.

function test(obj) {
    var iter = obj.iterator();
    assertEq(typeof iter, "object");
    assertEq(iter instanceof Iterator, true);
    assertEq(iter.toString(), "[object Iterator]");
}

test([]);
test(new Map);
test(new Set);
