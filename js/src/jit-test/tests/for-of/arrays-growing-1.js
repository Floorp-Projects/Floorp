// A for-of loop over an array continues to the end if the array grows during iteration.

var a = [0, 1, 1, 0, 1, 0, 0];
var s = '';
for (var v of a) {
    s += v;
    if (v === 1)
        a.push(2);
}
assertEq(s, '0110100222');
