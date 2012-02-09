// for-of works on arrays.

var a = ['a', 'b', 'c'];
var s = '';
for (var v of a)
    s += v;
assertEq(s, 'abc');
