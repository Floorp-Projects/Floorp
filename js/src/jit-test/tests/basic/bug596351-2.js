// |jit-test| error: TypeError

"use strict"
var g = newGlobal();

g.eval("bar = {}; Object.freeze(bar);");
g.bar.a = 4;
