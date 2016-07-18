// Settling a promise within an onPromiseSettled handler causes a recursive
// handler invocation.

var g = newGlobal();
var dbg = new Debugger(g);
var log;
var depth;

dbg.onPromiseSettled = function (promise) {
  log += '('; depth++;

  assertEq(promise.seen, undefined);
  promise.seen = true;

  if (depth < 3)
    g.settleFakePromise(g.makeFakePromise());

  log += ')'; depth--;
};

log = '';
depth = 0;
g.settleFakePromise(g.makeFakePromise());
assertEq(log, '((()))');
