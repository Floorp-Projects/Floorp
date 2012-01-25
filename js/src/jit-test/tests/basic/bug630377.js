var a = [];
for (var i = 0; i < 9; i++)
    a[i] = 0;
var b = [x === "0" && true for (x in a)];
assertEq(b[0], true);
for (var i = 1; i < 9; i++)
    assertEq(b[i], false);
