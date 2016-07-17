// onNewPromise handlers fire, until they are removed.
if (!('Promise' in this))
    quit(0);

var g = newGlobal();
var dbg = new Debugger(g);
var log;

log = '';
new g.Promise(function (){});
assertEq(log, '');

dbg.onNewPromise = function (promise) {
  log += 'n';
  assertEq(promise.seen, undefined);
  promise.seen = true;
};

log = '';
new g.Promise(function (){});
assertEq(log, 'n');

log = '';
dbg.onNewPromise = undefined;
new g.Promise(function (){});
assertEq(log, '');
