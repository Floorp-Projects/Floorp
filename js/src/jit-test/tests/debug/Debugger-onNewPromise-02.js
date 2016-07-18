// onNewPromise handlers fire, until they are removed.

var g = newGlobal();
var dbg = new Debugger(g);
var log;

log = '';
g.makeFakePromise();
assertEq(log, '');

dbg.onNewPromise = function (promise) {
  log += 'n';
  assertEq(promise.seen, undefined);
  promise.seen = true;
};

log = '';
g.makeFakePromise();
assertEq(log, 'n');

log = '';
dbg.onNewPromise = undefined;
g.makeFakePromise();
assertEq(log, '');
