function Ld(a) {
    var sum = 0;
    for (var d = 5; 0 <= d; d--)
        sum += a[d];
    return sum;
}
var arr = [0, 1, 2, 3, 4, 5, 6];

for (var i=0; i < 50; i++)
    assertEq(Ld(arr), 15);

var arr2 = [0, 1, 2, 3];
assertEq(Ld(arr2), NaN);
