// |jit-test| debug

var otherGlobal = newGlobal();
var f = otherGlobal.untrap;
f();
