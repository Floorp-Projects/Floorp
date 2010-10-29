var o = { get x() { return 17; } };
for (var j = 0; j < 5; ++j)
  o.x = 42;

assertEq(true, true);
