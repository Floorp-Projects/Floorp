var g = newGlobal('new-compartment');
g.eval('function f() { debugger; evaluate("debugger;", {newContext: true}); }');

var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame1) {
    dbg.onDebuggerStatement = function (frame2) {
        assertEq(frame1.eval("throw 'ponies'").throw, 'ponies');
        hits++;
    };
};

g.f();
assertEq(hits, 1);
