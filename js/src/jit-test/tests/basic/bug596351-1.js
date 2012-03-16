// |jit-test| error: TypeError
"use strict"
var g = newGlobal('new-compartment');
g.eval("foo = {}; Object.defineProperty(foo, 'a', {value: 2, writable: false});");
g.foo.a = 3;
