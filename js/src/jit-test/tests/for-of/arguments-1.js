// for-of can iterate arguments objects.

load(libdir + "iteration.js");

// Arguments objects do not have a .@@iterator() method by default.
// Install one on Object.prototype.
Object.prototype[std_iterator] = Array.prototype[std_iterator];

var s;
function test() {
    for (var v of arguments)
        s += v;
}

s = '';
test();
assertEq(s, '');

s = '';
test('x', 'y');
assertEq(s, 'xy');
