// String.prototype.iterator is generic.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

function test(obj) {
    var it = String.prototype[std_iterator].call(obj);
    for (var i = 0; i < (obj.length >>> 0); i++)
        assertIteratorResult(it.next(), obj[i], false);
    assertIteratorResult(it.next(), undefined, true);
}

test({length: 0});
test(Object.create(['x', 'y', 'z']));
test(Object.create({length: 2, 0: 'x', 1: 'y'}));
