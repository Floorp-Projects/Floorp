// Creating a promise within an onNewPromise handler causes a recursive handler
// invocation.

var g = newGlobal();
var dbg = new Debugger(g);
var log;
var depth;

dbg.onNewPromise = function (promise) {
  log += '('; depth++;

  assertEq(promise.seen, undefined);
  promise.seen = true;

  if (depth < 3)
    g.makeFakePromise();

  log += ')'; depth--;
};

log = '';
depth = 0;
g.makeFakePromise();
assertEq(log, '((()))');
