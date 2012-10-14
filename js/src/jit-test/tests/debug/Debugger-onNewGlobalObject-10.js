// An earlier onNewGlobalObject handler returning a 'throw' resumption
// value causes later handlers not to run.

load(libdir + 'asserts.js');

var dbg1 = new Debugger;
var dbg2 = new Debugger;
var dbg3 = new Debugger;
var log;
var count;

dbg1.onNewGlobalObject = dbg2.onNewGlobalObject = dbg3.onNewGlobalObject = function (global) {
  count++;
  log += count;
  if (count == 2)
    return { throw: "snoo" };
  return undefined;
};

log = '';
count = 0;
assertThrowsValue(function () { newGlobal(); }, "snoo");
assertEq(log, '12');
