// for-of can iterate arguments objects.

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
