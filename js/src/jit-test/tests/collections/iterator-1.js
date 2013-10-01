// collection.iterator() returns an Iterator object.

load(libdir + "iteration.js");

function test(obj, name) {
    var iter = obj[std_iterator]();
    assertEq(typeof iter, "object");
    assertEq(iter instanceof Iterator, true);
    assertEq(iter.toString(), "[object " + obj.constructor.name + " Iterator]");
}

// FIXME: Until arrays are converted to use the new iteration protocol,
// toString on this iterator doesn't work.  Bug 919948.
// test([]);
test(new Map);
test(new Set);
