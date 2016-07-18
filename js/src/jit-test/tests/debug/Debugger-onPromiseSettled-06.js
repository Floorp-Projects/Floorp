// Resumption values from onPromiseSettled handlers are disallowed.

load(libdir + 'asserts.js');

var g = newGlobal();
var dbg = new Debugger(g);
var log;

dbg.onPromiseSettled = function (g) { log += 's'; return undefined; };
log = '';
g.settleFakePromise(g.makeFakePromise());
assertEq(log, 's');

dbg.uncaughtExceptionHook = function (ex) { assertEq(/disallowed/.test(ex), true); log += 'u'; }
dbg.onPromiseSettled = function (g) { log += 's'; return { return: "snoo" }; };
log = '';
g.settleFakePromise(g.makeFakePromise());
assertEq(log, 'su');

dbg.onPromiseSettled = function (g) { log += 's'; return { throw: "snoo" }; };
log = '';
g.settleFakePromise(g.makeFakePromise());
assertEq(log, 'su');

dbg.onPromiseSettled = function (g) { log += 's'; return null; };
log = '';
g.settleFakePromise(g.makeFakePromise());
assertEq(log, 'su');

dbg.uncaughtExceptionHook = function (ex) { assertEq(/foopy/.test(ex), true); log += 'u'; }
dbg.onPromiseSettled = function (g) { log += 's'; throw "foopy"; };
log = '';
g.settleFakePromise(g.makeFakePromise());
assertEq(log, 'su');

