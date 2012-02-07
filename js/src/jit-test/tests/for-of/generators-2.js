// Generator-iterators are consumed the first time they are iterated.

function range(n) {
    for (var i = 0; i < n; i++)
        yield i;
}

var r = range(10);
var i = 0;
for (var x in r)
    assertEq(x, i++);
assertEq(i, 10);
for (var y in r)
    throw "FAIL";
assertEq(y, undefined);
