// The argument to Map can be a generator-expression.

var arr = [1, 2, "green", "red"];
var m = new Map([v, v] for (v of arr));
assertEq(m.size, 4);

for (var i = 0; i < 4; i++)
    assertEq(m.get(arr[i]), arr[i]);
