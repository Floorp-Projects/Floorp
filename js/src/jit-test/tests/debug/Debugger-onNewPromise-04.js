// An onNewPromise handler can disable itself.
var g = newGlobal({newCompartment: true});
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
