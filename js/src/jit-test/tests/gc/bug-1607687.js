var limit = 1 << 16;
var m = new Map;
for (var i = 1n; i < limit; i++)
  m.set(i, i);
