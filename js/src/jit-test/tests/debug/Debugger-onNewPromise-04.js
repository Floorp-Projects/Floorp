// An onNewPromise handler can disable itself.
if (!('Promise' in this))
    quit(0);

var g = newGlobal();
var dbg = new Debugger(g);
var log;

dbg.onNewPromise = function (promise) {
  log += 'n';
  dbg.onNewPromise = undefined;
};

log = '';
new g.Promise(function (){});
new g.Promise(function (){});
assertEq(log, 'n');
