// One Debugger's onNewGlobalObject handler can disable another Debugger's handler.

var dbg1 = new Debugger;
var dbg2 = new Debugger;
var dbg3 = new Debugger;
var log;
var hit;

function handler(global) {
  hit++;
  log += hit;
  if (hit == 2)
    dbg1.onNewGlobalObject = dbg2.onNewGlobalObject = dbg3.onNewGlobalObject = undefined;
};

log = '';
hit = 0;
dbg1.onNewGlobalObject = dbg2.onNewGlobalObject = dbg3.onNewGlobalObject = handler;
newGlobal();
assertEq(log, '12');
