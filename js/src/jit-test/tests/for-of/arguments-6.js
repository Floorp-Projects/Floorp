// Changing arguments.length affects a for-of loop iterating over arguments.

Object.prototype.iterator = Array.prototype.iterator;

var s;
function f() {
    arguments.length = 2;
    for (var v of arguments)
        s += v;
}

s = '';
f('a', 'b', 'c', 'd', 'e');
assertEq(s, 'ab');
