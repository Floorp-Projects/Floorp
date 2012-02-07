// for-of works with generators.

function range(n) {
    for (var i = 0; i < n; i++)
        yield i;
}

var s = '';
for (var a of range(4))
    s += a;
assertEq(s, '0123');
