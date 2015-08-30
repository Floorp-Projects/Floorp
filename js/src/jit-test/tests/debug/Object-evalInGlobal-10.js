var g = newGlobal();r
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

// executeInGlobal should be able to introduce and persist lexical bindings.
assertEq(gw.evalInGlobal(`let x = 42; x;`).return, 42);
assertEq(gw.evalInGlobal(`x;`).return, 42);

// By contrast, Debugger.Frame.eval is like direct eval, and shouldn't be able
// to introduce new lexical bindings.
dbg.onDebuggerStatement = function (frame) { frame.eval(`let y = 84;`); };
g.eval(`debugger;`);
assertEq(gw.evalInGlobal(`y;`).return, undefined);
