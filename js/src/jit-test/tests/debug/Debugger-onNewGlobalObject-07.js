// One Debugger's onNewGlobalObject handler can disable other Debuggers.

var dbg1 = new Debugger;
var dbg2 = new Debugger;
var dbg3 = new Debugger;
var log;
var hit;

function handler(global) {
  hit++;
  log += hit;
};

log = '';
hit = 0;
dbg1.onNewGlobalObject = dbg2.onNewGlobalObject = dbg3.onNewGlobalObject = handler;
newGlobal();
assertEq(log, '123');
