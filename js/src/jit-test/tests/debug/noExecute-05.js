// Tests that NX disallows debuggee execution for all debuggees.

load(libdir + "asserts.js");
load(libdir + "debuggerNXHelper.js");

var g1 = newGlobal();
var g2 = newGlobal();
var dbg = new Debugger;

dbg.addDebuggee(g1);
dbg.addDebuggee(g2);

g1.eval(`
        function f() { }
        var o = {
          get p() { },
          set p(x) { }
        };
        `);

g2.eval(`
        function f() { }
        var o = {
          get p() { },
          set p(x) { }
        };
        `);

var handlers = [() => { g1.f(); },
                () => { g1.o.p } ,
                () => { g1.o.p = 42; },
                () => { g2.f(); },
                () => { g2.o.p } ,
                () => { g2.o.p = 42; } ];

function testHook(hookName) {
  for (var h of handlers) {
    assertThrowsInstanceOf(h, Debugger.DebuggeeWouldRun);
  }
}

testDebuggerHooksNX(dbg, g1, testHook);
testDebuggerHooksNX(dbg, g2, testHook);
