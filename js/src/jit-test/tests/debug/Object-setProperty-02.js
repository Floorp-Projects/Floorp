// tests calling script functions via Debugger.Object.prototype.setProperty
// to see if they trigger debugger traps
"use strict";

var global = newGlobal();
var dbg = new Debugger(global);
dbg.onDebuggerStatement = onDebuggerStatement;

let obj;
global.eval(`
const obj = {
    set prop(val) {
        debugger;
        this._prop = val;
    }
};

debugger;
`);


function onDebuggerStatement(frame) {
    dbg.onDebuggerStatement = onDebuggerStatementGetter;

    obj = frame.environment.getVariable("obj");
}

let debuggerRan = false;

assertEq(obj.setProperty("prop", 42).return, true);
assertEq(obj.getProperty("_prop").return, 42);
assertEq(debuggerRan, true);

function onDebuggerStatementGetter(frame) {
    debuggerRan = true;
}