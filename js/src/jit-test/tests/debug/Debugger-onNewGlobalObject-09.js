// Resumption values from onNewGlobalObject handlers are respected.

load(libdir + 'asserts.js');

var dbg = new Debugger;
var log;

dbg.onNewGlobalObject = function (g) { log += 'n'; return undefined; };
log = '';
assertEq(typeof newGlobal(), "object");
assertEq(log, 'n');

// For onNewGlobalObject, { return: V } resumption values are treated like
// 'undefined': the new global is still returned.
dbg.onNewGlobalObject = function (g) { log += 'n'; return { return: "snoo" }; };
log = '';
assertEq(typeof newGlobal(), "object");
assertEq(log, 'n');

dbg.onNewGlobalObject = function (g) { log += 'n'; return { throw: "snoo" }; };
log = '';
assertThrowsValue(function () { newGlobal(); }, "snoo");
assertEq(log, 'n');

dbg.onNewGlobalObject = function (g) { log += 'n'; return null; };
log = '';
assertEq(evaluate('newGlobal();', { catchTermination: true }), "terminated");
