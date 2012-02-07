// for-of can iterate over typed arrays.

var a = Int8Array([0, 1, -7, 3])
var s = '';
for (var v of a)
    s += v + ',';
assertEq(s, '0,1,-7,3,');
