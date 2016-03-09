// Tests that invocation functions work.

load(libdir + "asserts.js");
load(libdir + "debuggerNXHelper.js");

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

g.eval(`
       function d() { debugger; }
       function f() { return 42; }
       var o = {
         get p() { return 42; },
         set p(x) { }
       };
       `);

var strs = ["f();", "o.p", "o.p = 42"];

var fw;
dbg.onDebuggerStatement = (frame) => {
  fw = frame.arguments[0];
};
gw.executeInGlobal("d(f)");
dbg.onDebuggerStatement = undefined;

function testHook(hookName) {
  var newestFrame = dbg.getNewestFrame();
  for (var s of strs) {
    if (newestFrame) {
      assertEq(newestFrame.eval(s).return, 42);
    }
    assertEq(gw.executeInGlobal(s).return, 42);
    assertEq(fw.apply(null).return, 42);
  }
}

testDebuggerHooksNX(dbg, g, testHook);
