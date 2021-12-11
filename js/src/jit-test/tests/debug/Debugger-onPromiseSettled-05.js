// Settling a promise within an onPromiseSettled handler causes a recursive
// handler invocation.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var log;
var depth;

dbg.onPromiseSettled = function (promise) {
  log += '('; depth++;

  assertEq(promise.seen, undefined);
  promise.seen = true;

  if (depth < 3) {
    gw.executeInGlobal(`settlePromiseNow(new Promise(_=>{}));`);
  }
  log += ')'; depth--;
};

log = '';
depth = 0;
g.settlePromiseNow(new g.Promise(_=>{}));
assertEq(log, '((()))');
