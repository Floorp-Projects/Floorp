// Changing arguments.length affects a for-of loop iterating over arguments.

load(libdir + "iteration.js");

Object.prototype[Symbol.iterator] = Array.prototype[Symbol.iterator];

var s;
function f() {
    arguments.length = 2;
    for (var v of arguments)
        s += v;
}

s = '';
f('a', 'b', 'c', 'd', 'e');
assertEq(s, 'ab');
