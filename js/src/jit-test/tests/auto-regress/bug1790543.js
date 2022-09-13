Array.prototype[0] = 0;

var a = [];
for (var i = 0; i < 10_000; ++i)
  a.push(0);

assertEq(a.length, 10_000);
