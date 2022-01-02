// Globals marked as invisibleToDebugger behave appropriately.

load(libdir + "asserts.js");

var dbg = new Debugger;
var log = '';
dbg.onNewGlobalObject = function (global) {
  log += 'n';
}

assertEq(typeof newGlobal(), "object");
assertEq(typeof newGlobal({invisibleToDebugger: false}), "object");
assertEq(log, 'nn');

log = '';
assertEq(typeof newGlobal({newCompartment: true, invisibleToDebugger: true}), "object");
assertEq(log, '');

assertThrowsInstanceOf(() => dbg.addDebuggee(newGlobal({newCompartment: true, invisibleToDebugger: true})),
                       Error);

var glob = newGlobal({newCompartment: true, invisibleToDebugger: true});
dbg.addAllGlobalsAsDebuggees();
dbg.onDebuggerStatement = function (frame) { assertEq(true, false); };
glob.eval('debugger');
