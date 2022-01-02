fullcompartmentchecks(true);
var g = newGlobal({
    newCompartment: true
});
g.eval("function*f(){debugger;yield}");
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function(frame) {};
g.f().next();
