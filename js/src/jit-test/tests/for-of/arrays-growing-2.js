// Inserting values in an array does not change the next index of an existing iterator.

var a = [1, 2, 3, 4];
var s = '';
for (var v of a) {
    s += v;
    if (s.length === 2)
        a.unshift('x');
}
assertEq(s, '12234');
