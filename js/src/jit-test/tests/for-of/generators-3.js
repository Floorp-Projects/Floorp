// Nested for-of loops can use the same generator-iterator.

function range(n) {
    for (var i = 0; i < n; i++)
        yield i;
}

var r = range(10);
for (var a of r)
    for (var b of r)
        for (var c of r)
            for (var d of r)
                ;

assertEq(a, 0);
assertEq(b, 1);
assertEq(c, 2);
assertEq(d, 9);
