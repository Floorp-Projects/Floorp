// Changing arguments.length during a for-of loop iterating over arguments affects the loop.

Object.prototype.iterator = Array.prototype.iterator;

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
