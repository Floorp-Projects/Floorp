// Debugger.prototype.addDebuggee should not accept invisible-to-debugger globals.
load(libdir + 'asserts.js');

var g = newGlobal({ invisibleToDebugger: true });

assertThrowsInstanceOf(() => { new Debugger(g); }, TypeError);
