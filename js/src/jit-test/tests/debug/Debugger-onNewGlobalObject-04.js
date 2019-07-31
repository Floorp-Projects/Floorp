// onNewGlobalObject handlers only fire on enabled Debuggers.

var dbg = new Debugger;
var log;

dbg.onNewGlobalObject = function (global) {
  log += 'n';
  assertEq(global.seen, undefined);
  global.seen = true;
};

log = '';
newGlobal();
assertEq(log, 'n');

log = '';
dbg.enabled = false;
newGlobal();
assertEq(log, '');

log = '';
dbg.enabled = true;
newGlobal();
assertEq(log, 'n');
