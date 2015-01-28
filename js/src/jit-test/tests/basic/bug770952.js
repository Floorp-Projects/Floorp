// |jit-test| error: TypeError
load(libdir + "iteration.js");

eval("var x; typeof x")
Array.prototype[Symbol.iterator] = function () { for(y in x); };
for (var v of ['a', 'b', 'c', 'd'])
    s = v;
