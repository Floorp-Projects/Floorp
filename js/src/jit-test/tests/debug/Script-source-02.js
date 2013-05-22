/*
 * Script.prototype.source should be the same object for both the top-level
 * script and the script of functions accessed as debuggee values on the global
 */
let g = newGlobal('new-compartment');
let dbg = new Debugger();
let gw = dbg.addDebuggee(g);

let count = 0;
dbg.onDebuggerStatement = function (frame) {
    ++count;
    assertEq(frame.script.source, gw.makeDebuggeeValue(g.f).script.source);
}

g.eval("function f() {}; debugger;");
assertEq(count, 1);
