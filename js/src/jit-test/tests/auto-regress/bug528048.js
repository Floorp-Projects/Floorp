// Binary: cache/js-dbg-32-761988dd0d81-linux
// Flags: -j
//
for (j = 0; j < 3; j++) {}
m = [];
m.concat();
n = [];
n.concat([]);
Function("\
  for (i = 0; i < 8; i++)\
  ((function f1(b, c) {\
  if (c) {\
    return (gc)()\
  }\
  f1(b, 1);\
  ((function f2(d, e) {\
    return d.length == e ? 0 : d[e] + f2(d, e + 1)\
  })([{}, /x/, /x/], 0))\
  })())\
")()
