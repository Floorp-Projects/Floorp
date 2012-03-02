// for-of visits each hole in an array full of holes.

var n = 0;
for (var x of Array(5)) {
    assertEq(x, undefined);
    n++;
}
assertEq(n, 5);
