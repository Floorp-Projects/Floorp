// for-of can iterate arguments objects after returning.

load(libdir + "iteration.js");

function f() {
    return arguments;
}

var s = '';
var args = f('a', 'b', 'c');
Object.prototype[Symbol.iterator] = Array.prototype[Symbol.iterator];
for (var v of args)
    s += v;
assertEq(s, 'abc');
