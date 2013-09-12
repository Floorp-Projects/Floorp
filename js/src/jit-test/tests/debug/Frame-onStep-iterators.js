var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var log;
var a = [];

dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  frame.onStep = function () {
    // This handler must not wipe out the debuggee's value in JSContext::iterValue.
    log += 's';
    // This will use JSContext::iterValue in the debugger.
    for (let i of a)
      log += 'i';
  };
};

log = '';
g.eval("debugger; for (let i of [1,2,3]) print(i);");
assertEq(!!log.match(/^ds*$/), true);
