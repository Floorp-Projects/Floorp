
x = 30;
assertEq(x, 30);

for (var i = 0; i < 10000; i++)
  this[i] = 0;

assertEq(x, 30);
