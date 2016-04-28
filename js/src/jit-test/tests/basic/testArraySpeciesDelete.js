delete Array[Symbol.species];
var a = [1, 2, 3].slice(1);
assertEq(a.length, 2);
assertEq(a[0], 2);
assertEq(a[1], 3);
