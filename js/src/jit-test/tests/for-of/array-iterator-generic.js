// Array.prototype.iterator is generic.
// That is, it can be applied to arraylike objects and strings, not just arrays.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

function test(obj) {
    var it = Array.prototype[std_iterator].call(obj);
    for (var i = 0; i < (obj.length >>> 0); i++)
        assertIteratorResult(it.next(), obj[i], false);
    assertIteratorResult(it.next(), undefined, true);
}

test({length: 0});
test({length: 0, 0: 'x', 1: 'y'});
test({length: 2, 0: 'x', 1: 'y'});
test(Object.create(['x', 'y', 'z']));
test(Object.create({length: 2, 0: 'x', 1: 'y'}));
test("");
test("ponies");

// Perverse length values.
test({length: 0x1f00000000});
test({length: -0xfffffffe, 0: 'a', 1: 'b'});
test({length: "011", 9: 9, 10: 10, 11: 11});
test({length: -0});
test({length: 2.7, 0: 0, 1: 1, 2: 2});
test({length: {valueOf: function () { return 3; }}, 0: 0, 1: 1, 2: 2});
