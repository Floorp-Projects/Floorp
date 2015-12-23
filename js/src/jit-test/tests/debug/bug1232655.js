g = newGlobal();
g.log = "";
Debugger(g).onDebuggerStatement = frame => frame.eval("log += this.Math.toString();");
g.eval("(function() { with ({}) debugger })()");
assertEq(g.log, "[object Math]");
