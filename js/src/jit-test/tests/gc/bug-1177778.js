// |jit-test| skip-if: !('setGCCallback' in this)

setGCCallback({
    action: "majorGC",
    phases: "both"
});
var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.eval("function h() { debugger; }");
dbg.onDebuggerStatement = function(hframe) {
    var env = hframe.older.environment;
};
g.eval("h();");

