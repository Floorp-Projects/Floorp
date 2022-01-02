// An onNewGlobalObject handler can disable itself.

var dbg = new Debugger;
var log;

dbg.onNewGlobalObject = function (global) {
  log += 'n';
  dbg.onNewGlobalObject = undefined;
};

log = '';
newGlobal();
assertEq(log, 'n');
