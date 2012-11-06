// Iterating over a set of objects yields those exact objects.

var arr = [{}, {}, {}, [], /xyz/, new Date];
var set = Set(arr);
assertEq(set.size, arr.length);

var i = 0;
for (var x of set)
    assertEq(x, arr[i++]);
assertEq(i, arr.length);

