// Array iterators keep the underlying array, arraylike object, or string alive.

load(libdir + "referencesVia.js");
load(libdir + "iteration.js");

function test(obj) {
    var it = Array.prototype[std_iterator].call(obj);
    assertEq(referencesVia(it, "**UNKNOWN SLOT 0**", obj), true);
}

test([]);
test([1, 2, 3, 4]);
test({});
