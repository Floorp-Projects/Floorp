// Breaking out of a for-of loop over a generator-iterator does not close the iterator.

load(libdir + "iteration.js");

function range(n) {
    for (var i = 0; i < n; i++)
        yield i;
}

var r = range(10);
var s = '';
for (var x of r) {
    s += x;
    if (x == 4)
        break;
}
s += '/';
for (var y of r)
    s += y;
assertEq(s, '01234/56789');
