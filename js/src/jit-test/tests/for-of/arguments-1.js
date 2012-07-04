// for-of can iterate arguments objects.

// Arguments objects do not have a .iterator() method by default.
// Install one on Object.prototype.
Object.prototype.iterator = Array.prototype.iterator;

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
