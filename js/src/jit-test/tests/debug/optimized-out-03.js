// Test that eval-in-frame throws on accessing optimized out values.

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_IonEagerNoOffthreadCompilation))
  quit(0);

withJitOptions(Opts_IonEagerNoOffthreadCompilation, function() {
  var dbgGlobal = newGlobal({newCompartment: true});
  var dbg = new dbgGlobal.Debugger();
  dbg.addDebuggee(this);

  function f() {
    assertEq(dbg.getNewestFrame().older.eval("print(a)").throw.unsafeDereference().toString(),
             "Error: variable `a' has been optimized out");
  }

  // Test optimized out binding in function scope.
  (function () {
    var a = 1;
    for (var i = 0; i < 1; i++) { f(); a = 2; }
  })();

  // Test optimized out binding in block scope.
  (function () {
    {
      let a = 1;
      for (var i = 0; i < 1; i++) { f(); a = 2; }
    }
  })();
});
