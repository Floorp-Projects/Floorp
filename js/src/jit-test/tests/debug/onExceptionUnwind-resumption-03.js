// Check that an onExceptionUnwind hook can force a frame to throw a different exception.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.onExceptionUnwind = function (frame, exc) {
    return { throw:"sproon" };
};
g.eval("function f() { throw 'ksnife'; }");
assertThrowsValue(g.f, "sproon");
