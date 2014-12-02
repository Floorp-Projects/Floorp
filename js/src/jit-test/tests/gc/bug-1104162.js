gczeal(11);
g = newGlobal()
g.eval("undefined;function f(){}")
Debugger(g).onDebuggerStatement = function(x) {
    x.eval("f").return.script.setBreakpoint(0, {})
}
g.eval("debugger")
