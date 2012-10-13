// Creating a global within an onNewGlobalObject handler causes a recursive handler invocation.
//
// This isn't really desirable behavior, as presumably a global created while a
// handler is running is one the debugger is creating for its own purposes and
// should not be observed, but if this behavior changes, we sure want to know.

var dbg = new Debugger;
var log;
var depth;

dbg.onNewGlobalObject = function (global) {
  log += '('; depth++;

  assertEq(global.seen, undefined);
  global.seen = true;

  if (depth < 3)
    newGlobal();

  log += ')'; depth--;
};

log = '';
depth = 0;
newGlobal();
assertEq(log, '((()))');
