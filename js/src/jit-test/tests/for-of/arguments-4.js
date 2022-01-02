// for-of can iterate arguments objects for other active frames.

load(libdir + "iteration.js");

Object.prototype[Symbol.iterator] = Array.prototype[Symbol.iterator];

var s;
function g(obj) {
    for (var v of obj)
        s += v;
}

function f() {
    g(arguments);
}

s = '';
f(1, 2, 3);
assertEq(s, '123');
