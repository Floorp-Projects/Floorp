// for-of can iterate arguments objects after returning.

load(libdir + "iteration.js");

function f() {
    return arguments;
}

var s = '';
var args = f('a', 'b', 'c');
Object.prototype[std_iterator] = Array.prototype[std_iterator];
for (var v of args)
    s += v;
assertEq(s, 'abc');
