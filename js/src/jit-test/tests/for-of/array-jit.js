var arr = [1, 2, 3];
var y = 0;
for (var i = 0; i < 10; i++)
    for (var x of arr)
        y += x;
assertEq(y, 60);
