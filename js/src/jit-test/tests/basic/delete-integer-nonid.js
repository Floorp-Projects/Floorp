var JSID_INT_MIN = -(1 << 30);
var JSID_INT_MAX = (1 << 30) - 1;

var o = {};


for (var i = 0; i < 10; i++)
  delete o[JSID_INT_MIN - 1];

for (var i = 0; i < 10; i++)
  delete o[JSID_INT_MIN];

for (var i = 0; i < 10; i++)
  delete o[JSID_INT_MIN + 1];


for (var i = 0; i < 10; i++)
  delete o[JSID_INT_MAX - 1];

for (var i = 0; i < 10; i++)
  delete o[JSID_INT_MAX];

for (var i = 0; i < 10; i++)
  delete o[JSID_INT_MAX + 1];
