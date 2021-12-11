var obj = { valueOf: {}, toString: function() { return 0; } };
for (var i = 0; i < 25; i++)
  assertEq(obj < 1, true);
