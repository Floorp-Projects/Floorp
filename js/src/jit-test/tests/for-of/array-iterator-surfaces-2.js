// Superficial tests for iterators created by Array.prototype.iterator

load(libdir + "iteration.js");

var proto = Object.getPrototypeOf([][std_iterator]());
assertEq(Object.getPrototypeOf(proto), Iterator.prototype);
proto = Object.getPrototypeOf([].keys());
assertEq(Object.getPrototypeOf(proto), Iterator.prototype);
proto = Object.getPrototypeOf([].entries());
assertEq(Object.getPrototypeOf(proto), Iterator.prototype);

function check(it) {
    assertEq(typeof it, 'object');
    assertEq(Object.getPrototypeOf(it), proto);
    assertEq(Object.getOwnPropertyNames(it).length, 0);
    assertEq(it[std_iterator](), it);

    // for-in enumerates the iterator's properties.
    it.x = 0;
    var s = '';
    for (var p in it)
        s += p + '.';
    assertEq(s, 'x.');
}

check([][std_iterator]());
check(Array.prototype[std_iterator].call({}));
check([].keys());
check(Array.prototype.keys.call({}));
check([].entries());
check(Array.prototype.entries.call({}));
