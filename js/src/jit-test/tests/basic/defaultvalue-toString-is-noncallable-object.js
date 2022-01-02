var obj = { toString: {}, valueOf: function() { return "foopy"; } };
for (var i = 0; i < 25; i++)
  assertEq(String(obj), "foopy");
