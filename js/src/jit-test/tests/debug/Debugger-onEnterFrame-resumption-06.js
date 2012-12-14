// |jit-test| mjitalways
// Bug 745194.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
g.eval("function f() {}");
dbg.onEnterFrame = function (frame) {
    if (frame.type == 'call') {
        gc();
        return { return: 'PASS' };
    }
};
assertEq(g.eval("f()"), 'PASS');
