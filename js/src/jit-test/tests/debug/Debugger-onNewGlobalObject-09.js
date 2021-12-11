// Resumption values from onNewGlobalObject handlers are disallowed.

load(libdir + 'asserts.js');

var dbg = new Debugger;
var log;

dbg.onNewGlobalObject = function (g) { log += 'n'; return undefined; };
log = '';
assertEq(typeof newGlobal(), "object");
assertEq(log, 'n');

dbg.uncaughtExceptionHook = function (ex) { assertEq(/disallowed/.test(ex), true); log += 'u'; }
dbg.onNewGlobalObject = function (g) { log += 'n'; return { return: "snoo" }; };
log = '';
assertEq(typeof newGlobal(), "object");
assertEq(log, 'nu');

dbg.onNewGlobalObject = function (g) { log += 'n'; return { throw: "snoo" }; };
log = '';
assertEq(typeof newGlobal(), "object");
assertEq(log, 'nu');

dbg.onNewGlobalObject = function (g) { log += 'n'; return null; };
log = '';
assertEq(typeof newGlobal(), "object");
assertEq(log, 'nu');

dbg.uncaughtExceptionHook = function (ex) { assertEq(/foopy/.test(ex), true); log += 'u'; }
dbg.onNewGlobalObject = function (g) { log += 'n'; throw "foopy"; };
log = '';
assertEq(typeof newGlobal(), "object");
assertEq(log, 'nu');

