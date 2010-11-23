// |jit-test| error: RangeError;
var vs = [1, 1, 1, 1, 3.5, 1];
for (var i = 0, sz = vs.length; i < sz; i++) new Array(vs[i]);
