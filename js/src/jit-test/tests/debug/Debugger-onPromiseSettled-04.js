// An onPromiseSettled handler can disable itself.

var g = newGlobal();
var dbg = new Debugger(g);
var log;

dbg.onPromiseSettled = function (promise) {
  log += 's';
  dbg.onPromiseSettled = undefined;
};

log = '';
g.settleFakePromise(g.makeFakePromise());
g.settleFakePromise(g.makeFakePromise());
assertEq(log, 's');
