// Resumption values from uncaughtExceptionHook from onNewGlobalObject handlers are respected.

load(libdir + 'asserts.js');

var dbg = new Debugger;
var log;

dbg.onNewGlobalObject = function () {
  log += 'n';
  throw 'party';
};

dbg.uncaughtExceptionHook = function (ex) {
  log += 'u';
  assertEq(ex, 'party');
  return { throw: 'fit' };
};

log = '';
assertThrowsValue(newGlobal, 'fit');
assertEq(log, 'nu');

dbg.uncaughtExceptionHook = function (ex) {
  log += 'u';
  assertEq(ex, 'party');
};

log = '';
assertEq(typeof newGlobal(), 'object');
assertEq(log, 'nu');
