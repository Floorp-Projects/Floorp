// String.prototype.iterator is generic.

load(libdir + "asserts.js");

function test(obj) {
    var it = Array.prototype.iterator.call(obj);
    for (var i = 0; i < (obj.length >>> 0); i++)
        assertEq(it.next(), obj[i]);
    assertThrowsValue(function () { it.next(); }, StopIteration);
}

test({length: 0});
test(Object.create(['x', 'y', 'z']));
test(Object.create({length: 2, 0: 'x', 1: 'y'}));
