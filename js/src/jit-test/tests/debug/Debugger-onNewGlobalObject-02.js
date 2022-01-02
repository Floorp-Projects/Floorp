// onNewGlobalObject handlers fire, until they are removed.

var dbg = new Debugger;
var log;

log = '';
newGlobal();
assertEq(log, '');

dbg.onNewGlobalObject = function (global) {
  log += 'n';
  assertEq(global.seen, undefined);
  global.seen = true;
};

log = '';
newGlobal();
assertEq(log, 'n');

log = '';
dbg.onNewGlobalObject = undefined;
newGlobal();
assertEq(log, '');
