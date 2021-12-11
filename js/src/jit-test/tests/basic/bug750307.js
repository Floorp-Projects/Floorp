load(libdir + "asserts.js");
var g = newGlobal();
var a = g.RegExp("x");
Object.defineProperty(a, "ignoreCase", {value: undefined});
a.toString();
