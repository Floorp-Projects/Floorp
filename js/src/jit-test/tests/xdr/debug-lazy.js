load(libdir + 'bytecode-cache.js');

// Ensure that if a function is encoded when non-lazy but relazifiable, then
// decoded, the resulting LazyScript is marked as being non-lazy so that when
// the debugger tries to delazify things it doesn't get all confused.  We just
// use findScripts() to trigger debugger delazification; we don't really care
// about the scripts themselves.
function checkAfter(ctx) {
    var dbg = new Debugger(ctx.global);
    var allScripts = dbg.findScripts();
    assertEq(allScripts.length == 0, false);
}

test = `
  function f() { return true; };
  f();
  `
evalWithCache(test, { assertEqBytecode: true, assertEqResult: true,
		      checkAfter: checkAfter });
