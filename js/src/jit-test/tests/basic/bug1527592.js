// |jit-test| error:dead object
var g1 = newGlobal();
var g2 = newGlobal({newCompartment: true});
var f = g2.Function("");
nukeAllCCWs();
var c = new class extends f {};
c();
