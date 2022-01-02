// Resumption values from uncaughtExceptionHook from onNewGlobalObject
// handlers do not affect the dispatch of the event to other Debugger instances.

load(libdir + 'asserts.js');

var dbg1 = new Debugger;
var dbg2 = new Debugger;
var dbg3 = new Debugger;
var log;

dbg1.onNewGlobalObject = dbg2.onNewGlobalObject = dbg3.onNewGlobalObject = function () {
  log += 'n';
  throw 'party';
};

dbg1.uncaughtExceptionHook = dbg2.uncaughtExceptionHook = dbg3.uncaughtExceptionHook =
function (ex) {
  log += 'u';
  assertEq(ex, 'party');
  return { throw: 'fit' };
};

log = '';
assertEq(typeof newGlobal(), 'object');
assertEq(log, 'nununu');
