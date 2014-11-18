// An onNewPromise handler can disable itself.

var g = newGlobal();
var dbg = new Debugger(g);
var log;

dbg.onNewPromise = function (promise) {
  log += 'n';
  dbg.onNewPromise = undefined;
};

log = '';
g.makeFakePromise();
g.makeFakePromise();
assertEq(log, 'n');
