var g1 = newGlobal({newCompartment: true});
var g2 = newGlobal({newCompartment: true});
g1.eval("function f1() { debugger; evaluate('debugger') }");
g2.eval("function f2() { f1(); assertEq(Number(this), 42) }");
g2.f1 = g1.f1;

var dbg = new Debugger(g1,g2);
dbg.onDebuggerStatement = function(frame) {
    var target = frame.older;
    dbg.onDebuggerStatement = function(frame) {
        assertEq(Number(target.this.unsafeDereference()), 42);
    }
}
g2.f2.call(42);
