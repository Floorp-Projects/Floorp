// for-of can iterate strict arguments objects in non-strict code.

Object.prototype.iterator = Array.prototype.iterator;

var s;
function g(obj) {
    for (var v of obj)
        s += v;
}

function f() {
    "use strict";
    g(arguments);
}

s = '';
f(1, 2, 3);
assertEq(s, '123');
