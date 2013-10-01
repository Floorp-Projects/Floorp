// |jit-test| error: TypeError
eval("var x; typeof x")
Array.prototype.iterator = function () { for(y in x); };
for (var v of ['a', 'b', 'c', 'd'])
    s = v;
