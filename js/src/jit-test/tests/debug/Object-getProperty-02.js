// tests calling script functions via Debugger.Object.prototype.getProperty
// to see if they trigger debugger traps
"use strict";

var global = newGlobal();
var dbg = new Debugger(global);
dbg.onDebuggerStatement = onDebuggerStatement;

let obj;
global.eval(`
const obj = {
    get prop() {
        debugger;
        return 42;
    }
};

debugger;
`);


function onDebuggerStatement(frame) {
    dbg.onDebuggerStatement = onDebuggerStatementGetter;

    obj = frame.environment.getVariable("obj");
}

let debuggerRan = false;

assertEq(obj.getProperty("prop").return, 42);
assertEq(debuggerRan, true);

function onDebuggerStatementGetter(frame) {
    debuggerRan = true;
}