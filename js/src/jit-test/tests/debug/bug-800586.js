var g = newGlobal('new-compartment');
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
dbg.onDebuggerStatement = function (f) {
	gw.evalInGlobal("eval('var x = \"A Brief History of Love\"');\n")
};
g.eval('debugger');
