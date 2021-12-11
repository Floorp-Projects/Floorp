// for-of works on slow arrays.

var a = ['a', 'b', 'c'];
a.slow = true;
var log = '';
for (var x of a)
    log += x;
assertEq(log, 'abc');
