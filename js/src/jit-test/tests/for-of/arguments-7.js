// Changing arguments.length during a for-of loop iterating over arguments affects the loop.

load(libdir + "iteration.js");

Object.prototype[std_iterator] = Array.prototype[std_iterator];

var s;
function f() {
    for (var v of arguments) {
        s += v;
        arguments.length--;
    }
}

s = '';
f('a', 'b', 'c', 'd', 'e');
assertEq(s, 'abc');
