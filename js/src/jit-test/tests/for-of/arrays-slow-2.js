// Two for-of loops on the same slow array get distinct iterators.

var a = [1, 2, 3];
a.slow = true;
var s = '';
for (var x of a)
    s += x;
for (var y of a)
    s += y;
assertEq(s, '123123');
