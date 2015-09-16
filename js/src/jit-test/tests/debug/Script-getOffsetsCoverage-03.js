// |jit-test| error: Error: can't start debugging: a debuggee script is on the stack

var g = newGlobal();
var dbg = Debugger(g);
function loop(i) {
  var n = 0;
  for (n = 0; n < i; n++)
    debugger;
}
g.eval(loop.toSource());

var countDown = 20;
dbg.onDebuggerStatement = function (f) {
  // Should throw an error.
  if (countDown > 0 && --countDown == 0) {
    dbg.collectCoverageInfo = !dbg.collectCoverageInfo;
  }
};

dbg.collectCoverageInfo = false;
g.eval("loop("+ (2 * countDown) +");");
