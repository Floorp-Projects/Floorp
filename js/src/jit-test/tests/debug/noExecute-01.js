// Tests that NX disallows debuggee execution for all the hooks.

load(libdir + "asserts.js");
load(libdir + "debuggerNXHelper.js");

var g = newGlobal();
var dbg = new Debugger(g);

// Attempts to call g.f without going through an invocation function should
// throw.
g.eval(`
       function f() { }
       var o = {
         get p() { },
         set p(x) { }
       };
       `);

var handlers = [() => { g.f(); },
                () => { g.o.p } ,
                () => { g.o.p = 42; }];

function testHook(hookName) {
  for (var h of handlers) {
    assertThrowsInstanceOf(h, Debugger.DebuggeeWouldRun);
  }
}

testDebuggerHooksNX(dbg, g, testHook);
