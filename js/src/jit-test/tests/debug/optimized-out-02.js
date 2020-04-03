// Test that prevUpToDate on frames are cleared.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

g.eval(`
function outer(unaliasedArg) {
  var unaliasedVar = unaliasedArg + 42;
  var aliasedVar = unaliasedArg;

  inner();
  return unaliasedVar; // To prevent the JIT from optimizing out unaliasedVar.

  function inner() {
    aliasedVar++;
  }
}
`);

var log = "";
for (var script of dbg.findScripts()) {
  if (script.displayName === "inner") {
    script.setBreakpoint(0, { hit: function(frame) {
      // Force updateLiveScopes.
      var outerEnv = frame.environment;

      // Get the environment of outer's frame on the stack, so that we may
      // recover unaliased bindings in the debug scope.
      outerEnv = frame.older.environment;
      log += outerEnv.getVariable('unaliasedArg'); // 42
      log += outerEnv.getVariable('unaliasedVar'); // 84
      log += outerEnv.getVariable('aliasedVar');   // 42
    }});
  }
}

g.outer(42);
assertEq(log, "428442");
