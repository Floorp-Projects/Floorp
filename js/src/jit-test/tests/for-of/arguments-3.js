// for-of can iterate strict arguments objects.

var s;
function test() {
    "use strict";
    for (var v of arguments)
        s += v;
}

s = '';
test();
assertEq(s, '');

s = '';
test('a', 'b');
assertEq(s, 'ab');
