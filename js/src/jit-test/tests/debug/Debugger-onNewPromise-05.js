// Creating a promise within an onNewPromise handler causes a recursive handler
// invocation.
if (!('Promise' in this))
    quit(0);

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var log;
var depth;

dbg.onNewPromise = function (promise) {
  log += '('; depth++;

  assertEq(promise.seen, undefined);
  promise.seen = true;

  if (depth < 3)
      gw.executeInGlobal(`new Promise(_=>{})`);

  log += ')'; depth--;
};

log = '';
depth = 0;
new g.Promise(function (){});
assertEq(log, '((()))');
