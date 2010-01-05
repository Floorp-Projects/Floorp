var x;
var str = "a";
assertEq(str.charCodeAt(Infinity) | 0, 0);
for (var i = 0; i < 10; ++i)
  x = str.charCodeAt(Infinity) | 0;
assertEq(x, 0);
for (var i = 0; i < 10; ++i)
  x = str.charCodeAt(Infinity);
assertEq(x | 0, 0);

