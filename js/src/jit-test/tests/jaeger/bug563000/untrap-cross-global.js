// |jit-test| debug

var otherGlobal = newGlobal('new-compartment');
var f = otherGlobal.untrap;
f();
