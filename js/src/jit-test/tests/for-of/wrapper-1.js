// for-of works on cross-compartment wrappers of Arrays.

var g = newGlobal('new-compartment');
var s = '';
for (var x of g.Array(1, 1, 2, 3, 5))
    s += x;
assertEq(s, '11235');
