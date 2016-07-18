// onPromiseSettled handlers fire, until they are removed.

var g = newGlobal();
var dbg = new Debugger(g);
var log;

log = '';
g.settleFakePromise(g.makeFakePromise());
assertEq(log, '');

dbg.onPromiseSettled = function (promise) {
  log += 's';
  assertEq(promise.seen, undefined);
  promise.seen = true;
};

log = '';
g.settleFakePromise(g.makeFakePromise());
assertEq(log, 's');

log = '';
dbg.onPromiseSettled = undefined;
g.settleFakePromise(g.makeFakePromise());
assertEq(log, '');
