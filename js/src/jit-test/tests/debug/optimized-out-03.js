// Test that eval-in-frame throws on accessing optimized out values.

// Use gczeal 0 to keep CGC from invalidating Ion code and causing test failures.
gczeal(0);

load(libdir + "jitopts.js");

if (!jitTogglesMatch(Opts_IonEagerNoOffthreadCompilation))
  quit(0);

withJitOptions(Opts_IonEagerNoOffthreadCompilation, function() {
  var dbgGlobal = newGlobal({newCompartment: true});
  var dbg = new dbgGlobal.Debugger();
  dbg.addDebuggee(this);

  var warmedUp = false;
  function check() {
    if (warmedUp) {
      var a = dbg.getNewestFrame().older.eval("a")
      assertEq(a.throw.unsafeDereference().toString(),
               "Error: variable 'a' has been optimized out");
    }
  }

  // Test optimized-out binding in function scope.
  function testFunctionScope() {
    var a = 1;
    for (var i = 0; i < 1; i++) { check(); }
  }

  // Test optimized-out binding in block scope.
  function testBlockScope() {
    {
      let a = 1;
      for (var i = 0; i < 1; i++) { check(); }
    }
  }

  with({}) {}

  testFunctionScope();
  testBlockScope();

  warmedUp = true;

  testFunctionScope();
  testBlockScope();
});
