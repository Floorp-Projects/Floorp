// Resumption values from onPromiseSettled handlers are disallowed.
if (!('Promise' in this))
    quit(0);

load(libdir + 'asserts.js');

var g = newGlobal();
var dbg = new Debugger(g);
var log;

dbg.onPromiseSettled = function (g) { log += 's'; return undefined; };
log = '';
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log, 's');

dbg.uncaughtExceptionHook = function (ex) { assertEq(/disallowed/.test(ex), true); log += 'u'; }
dbg.onPromiseSettled = function (g) { log += 's'; return { return: "snoo" }; };
log = '';
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log, 'su');

dbg.onPromiseSettled = function (g) { log += 's'; return { throw: "snoo" }; };
log = '';
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log, 'su');

dbg.onPromiseSettled = function (g) { log += 's'; return null; };
log = '';
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log, 'su');

dbg.uncaughtExceptionHook = function (ex) { assertEq(/foopy/.test(ex), true); log += 'u'; }
dbg.onPromiseSettled = function (g) { log += 's'; throw "foopy"; };
log = '';
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log, 'su');

